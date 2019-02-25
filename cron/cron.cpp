#include <eosiolib/eosio.hpp>
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

    ACTION start(name from) {
        stop_execution.set(false, from);
        print("Set START");
    }

    ACTION stop(name from) {
        stop_execution.set(true, from);
        print("Set STOP");
    }

    ACTION schedule(name from, name account, string action, uint32_t period) {
        require_auth(from);
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
            row.active = true;
        });
        print("Job id: ", pk, "\n");
    }

    ACTION run(name from, uint32_t polling_interval) {
        require_auth(get_self());
        if (stop_execution.get())
            return;

        scan_table();
        create_transaction(_self, _self, "run", polling_interval, make_tuple(_self, polling_interval));
    }

    ACTION enable(name from, uint64_t job_id) {
        change_status(from, job_id, true);
    }

    ACTION disable(name from, uint64_t job_id) {
        change_status(from, job_id, false);
    }

    cron(name receiver, name code, datastream<const char*> ds):
            contract(receiver, code, ds), stop_execution(_self, _self.value), unique_id(_self, _self.value) {

        if (!stop_execution.exists()) {
            stop_execution.set(false, _code);
        }

        if (!unique_id.exists()) {
            unique_id.set(0, _code);
        }
    }

    void scan_table() {
        print("Scanning timetable\n");
        timetable cron_table(_code, _code.value);
        for (auto iterator = cron_table.begin(); iterator != cron_table.end(); iterator++) {
            auto& item = *iterator;
            print("Processing ", item.key, "\n");
            if (time_point_sec(now()) >= item.next_run) {
                // Empty Args
                create_transaction(item.from, item.account, item.action, item.period, tuple<name>(item.from));
                cron_table.modify(iterator, _code, [&](auto& row) {
                    row.next_run = item.next_run + item.period;
                });
            }
        }
    }

    template<class ...TParams>
    void create_transaction(name payer, name account, const string &action, uint32_t delay,
            const std::tuple<TParams...>& args) {
        print_f("Creating transaction: payer=% target=[%:%] delay=%\n", payer, account, action, delay);
        eosio::transaction t;

        t.actions.emplace_back(
                permission_level(_self, "active"_n),
                account,
                name(action),
                args);

        t.delay_sec = delay;

        auto sender_id = unique_id.get();
        t.send(sender_id, payer);
        print("Scheduled with a delay of ", delay, "\n");
        unique_id.set(sender_id + 1, _code);
    }

    void change_status(name from, uint64_t job_id, bool active) {
        timetable cron_table(_code, _code.value);
        auto iterator = cron_table.find(job_id);
        if (iterator != cron_table.end()) {
            cron_table.modify(iterator, from, [&]( auto& row ) {
                row.active = active;
            });
        }
    }

private:
    TABLE timetable_data {
        uint64_t key;
        name from;
        name account;
        string action;
        uint32_t period;
        time_point_sec next_run;
        bool active;

        uint64_t primary_key() const { return key;}
        uint64_t by_from() const { return from.value; }
    };

    typedef eosio::multi_index<"timetable"_n,
                                timetable_data,
                                eosio::indexed_by<"from"_n, eosio::const_mem_fun<timetable_data,
                                                                                 uint64_t ,
                                                                                 &timetable_data::by_from>>> timetable;
    eosio::singleton<"flag"_n, bool> stop_execution;
    eosio::singleton<"increment"_n, uint64_t> unique_id;
};

EOSIO_DISPATCH(cron, (dumb)(start)(stop)(schedule)(run))