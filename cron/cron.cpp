#include <eosiolib/asset.hpp>
#include <eosiolib/transaction.hpp>
#include <eosiolib/singleton.hpp>

using std::make_tuple;
using std::tuple;
using std::string;
using namespace eosio;

/*
 * TODO
 * Error handling (retry failed transaction) - implement onError
 * Store action's args in the timetable_data
 * ACTION for updating polling_interval
 * Get rid of dumb action
*/

CONTRACT cron : public contract {
public:
    using contract::contract;

    ACTION dumb(name from) {
        print("THE DUMB ACTION CALLED by ", from);
    }

    ACTION setbalance(name from, uint64_t amount) {
        require_auth(get_self());
        balance_sheet balance(_self, _self.value);
        balance.modify(balance.find(from.value), _code, [&](auto& row) {
            row.balance = amount;
        });
    }

    ACTION start() {
        require_auth(get_self());
        stop_execution.set(false, _code);
        print("Set START");
    }

    ACTION stop() {
        require_auth(get_self());
        stop_execution.set(true, _code);
        print("Set STOP");
    }

    ACTION schedule(name from, name account, string action, uint32_t period) {
        require_auth(from);
        check(get_balance(from) >= CALL_PRICE * PREPAY_CALL_AMOUNT, "Insufficient balance to schedule a task");
        reduce_balance(from, CALL_PRICE * PREPAY_CALL_AMOUNT);
        time_point_sec current_time(now());
        timetable cron_table(_code, _code.value);
        const auto pk = cron_table.available_primary_key();
        cron_table.emplace(from, [&]( auto& row ) {
            row.key = pk;
            row.from = from;
            row.account = account;
            row.action = action;
            row.period = period;
            row.next_run = current_time + period;
            row.last_updated = current_time;
            row.active = true;
        });
        print("Job id: ", pk, "\n");
    }

    ACTION run(uint32_t polling_interval, uint32_t rows_count) {
        require_auth(get_self());
        if (stop_execution.get())
            return;

        scan_schedules(rows_count);
        create_transaction(_self, _self, "run", polling_interval, make_tuple(polling_interval, rows_count));
    }

    ACTION enable(name from, uint64_t job_id) {
        change_status(from, job_id, true);
    }

    ACTION disable(name from, uint64_t job_id) {
        change_status(from, job_id, false);
    }

    ACTION deposit(name from, name to, asset quantity, string memo) {
        add_balance(from, quantity.amount);
    }

    ACTION withdraw(name from, uint64_t amount) {
        require_auth(from);
        check(get_balance(from) >= amount, "Withdraw amount is larger than wallet balance");
        action(
                permission_level(_self, "active"_n),
                "eosio.token"_n,
                "transfer"_n,
                std::make_tuple(_self, from, asset(amount,
                                                   symbol("CRON", 0)), std::string("cronos_withraw"))
        ).send();
    }

    cron(name receiver, name code, datastream<const char*> ds):
            contract(receiver, code, ds), stop_execution(_self, _self.value), unique_id(_self, _self.value) {
        if (!stop_execution.exists()) {
            stop_execution.set(false, _self);
        }

        if (!unique_id.exists()) {
            unique_id.set(0, _self);
        }
    }

    void scan_schedules(uint32_t rows_count) {
        print("Scanning timetable\n");
        timetable cron_table(_self, _self.value);
        for (const auto& item : cron_table.get_index<"lastupdated"_n>()) {
            if (!rows_count) {
                break;
            }
            print("Processing ", item.key, "\n");
            time_point_sec current_time(now());
            cron_table.modify(item, _self, [&](auto& row) {
                row.last_updated = current_time;
            });

            if (!item.active) {
                continue;
            }

            if (current_time >= item.next_run) {
                const name& account_from = item.from;

                if (get_balance(account_from) >= CALL_PRICE) {
                    reduce_balance(account_from, CALL_PRICE);
                    create_transaction(account_from, item.account, item.action, item.period, tuple<name>(account_from));
                }

                cron_table.modify(item, _self, [&](auto& row) {
                    row.next_run = item.next_run + item.period;
                });
            }
            --rows_count;
        }
    }

    template<class ...TParams>
    void create_transaction(name payer, name account, const string &action, uint32_t delay,
            const std::tuple<TParams...>& args) {
        print_f("Creating transaction: payer=% target=[%:%] delay=%\n", payer, account, action, delay);
        eosio::transaction t;
        t.actions.emplace_back(
                permission_level(_code, "active"_n),
                account,
                name(action),
                args);

        t.delay_sec = delay;

        auto sender_id = unique_id.get();
        t.send(sender_id, _code);
        print("Scheduled with a delay of ", delay, "\n");
        unique_id.set(sender_id + 1, _code);
    }

    void change_status(name from, uint64_t job_id, bool active) {
        timetable cron_table(_code, _code.value);
        cron_table.modify(cron_table.require_find(job_id), from, [&]( auto& row ) {
            row.active = active;
        });
    }

    void update_balance(name from, int64_t amount) {
        balance_sheet balance(_self, _self.value);
        auto iterator = balance.find(from.value);
        if (iterator != balance.end()) {
            balance.modify(iterator, _self, [&](auto& row) {
                row.balance += amount;
            });
        } else {
            balance.emplace(_self, [&](auto& row) {
                row.account = from;
                row.balance = amount;
            });
        }
    }

    void add_balance(name from, uint64_t amount) {
        // Overflow check
        uint64_t balance = get_balance(from);
        check(balance + amount >= balance, "Int64 Overflow error");
        update_balance(from, amount);
    }

    void reduce_balance(name from, uint64_t amount) {
        update_balance(from, -1 * static_cast<int64_t>(amount));
    }

    uint64_t get_balance(name from) {
        balance_sheet balance(_self, _self.value);
        auto iterator = balance.find(from.value);
        return iterator != balance.end() ? iterator->balance : 0;
    }

private:
    TABLE timetable_data {
        uint64_t key;
        name from;
        name account;
        string action;
        uint32_t period;
        time_point_sec last_updated;
        time_point_sec next_run;
        bool active;

        uint64_t primary_key() const { return key;}
        uint64_t by_last_updated() const { return last_updated.utc_seconds; }
    };

    TABLE balance_data {
        name account;
        uint64_t balance;

        uint64_t primary_key() const { return account.value; }
    };

    typedef eosio::multi_index<"timetable"_n,
                               timetable_data,
                               eosio::indexed_by<"lastupdated"_n,
                                                 eosio::const_mem_fun<timetable_data,
                                                                      uint64_t,
                                                                      &timetable_data::by_last_updated>>> timetable;

    typedef eosio::multi_index<"balance"_n, balance_data> balance_sheet;

    eosio::singleton<"flag"_n, bool> stop_execution;
    eosio::singleton<"increment"_n, uint64_t> unique_id;

    const uint32_t PREPAY_CALL_AMOUNT = 5;
    const uint32_t CALL_PRICE = 10;
};

extern "C" {
   void apply(uint64_t receiver, uint64_t code, uint64_t action) {
      if (code == receiver) {
          switch (action) {
              EOSIO_DISPATCH_HELPER(cron,
                                    (dumb)(setbalance)(enable)(disable)(deposit)(withdraw)(start)(stop)(schedule)(run))
          }
      } else if (code == name("eosio.token").value && action == name("transfer").value) {
            execute_action(name(receiver), name(code), &cron::deposit);
      }
   }
}
