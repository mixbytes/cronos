const Helpers = require('eos-test-helper');

const expect = require('chai').expect;
require('chai').should();

const CONTRACT_DIR = '../build';

describe('full cycle', () => {

    let sender_name = Helpers.random_name();

    let contract_account = null;
    let sender_account = null;
    let deposit_amount = 500;
    let schedule_period = 3;

    it('create contract_account', async function () {
        contract_account = {
            name: Helpers.random_name(),
            ... await Helpers.random_keys()
        };
        await Helpers.new_account(contract_account);
        console.log('Conract account: ' + contract_account.name + '\n');
    });

    it('create sender_account', async function () {
        sender_account = {
            name: sender_name,
            ... await Helpers.random_keys()
        };
        await Helpers.new_account(sender_account);
        console.log('Sender account: ' + sender_account.name + '\n');
    });

    it('deploy contract', async function() {
        await Helpers.deploy(contract_account, Helpers.load_contract(CONTRACT_DIR, 'cron'));
    })

    it('dumb', async function() {
        await Helpers.api([contract_account.private_key]).transact({
            actions: [{
                account: contract_account.name,
                name: 'dumb',
                data: {"from": sender_account.name},
                authorization: [{
                    actor: contract_account.name,
                    permission: 'active',
                }]
            }]
        }, {
            blocksBehind: 3,
            expireSeconds: 120,
        });
    });

    it('schedule insufficient_balance_error', async function() {
        let fail = false;
        try {
            api = await Helpers.api([sender_account.private_key]).transact({
                actions: [{
                    account: contract_account.name,
                    name: 'schedule',
                    data: {
                        from: sender_account.name,
                        account: contract_account.name,
                        action: 'dumb',
                        period: 3,
                    },
                    authorization: [{
                        actor: sender_account.name,
                        permission: 'active',
                    }],
                }]
            }, {
                blocksBehind: 3,
                expireSeconds: 120,
            });
        } catch (e) {
            fail = true;
        }
        expect(fail).be.true;
    });

    it('create CRON', async function() {
        await Helpers.api().transact({
            actions: [{
                account: 'eosio.token',
                name: 'create',
                data: {
                    issuer: 'eosio.token',
                    maximum_supply: '100000 CRON',
                },
                authorization: [{
                    actor: 'eosio.token',
                    permission: 'active',
                }]
            }]
        }, {
            blocksBehind: 3,
            expireSeconds: 120,
        }); 
    })

    it('issue CRON', async function() {
        await Helpers.api().transact({
            actions: [{
                account: 'eosio.token',
                name: 'issue',
                data: {
                    to: sender_account.name,
                    quantity: '10000 CRON',
                    memo: 'issue CRON',
                },
                authorization: [{
                    actor: 'eosio.token',
                    permission: 'active',
                }]
            }]
        }, {
            blocksBehind: 3,
            expireSeconds: 120,
        }); 
    })

    it('deposit', async function() {
        let api = Helpers.api([sender_account.private_key]);
        await api.transact({
            actions: [{
                account: contract_account.name,
                name: 'deposit',
                data: {
                    from: sender_account.name,
                    to: contract_account.name,
                    quantity: `${deposit_amount} cron`,
                    memo: 'deposit cron'
                },
                authorization: [{
                    actor: sender_account.name,
                    permission: 'active',
                }],
            }]
        }, {
            blocksBehind: 3,
            expireSeconds: 120,
        });
        let data = await api.rpc.get_table_rows({
            json: true,
            code: contract_account.name,
            scope: contract_account.name,
            table: 'balance',
        }), rows = data['rows'];
        console.log(rows);
        expect(rows[0]['balance']).to.equal(deposit_amount);
    })

    it('schedule', async function() {
        let api = Helpers.api([sender_account.private_key]);
        await api.transact({
            actions: [{
                account: contract_account.name,
                name: 'schedule',
                data: {
                    from: sender_account.name,
                    account: contract_account.name,
                    action: 'dumb',
                    period: schedule_period,
                },
                authorization: [{
                    actor: sender_account.name,
                    permission: 'active',
                }],
            }]
        }, {
            blocksBehind: 3,
            expireSeconds: 120,
        });
        
        let data = await Helpers.api([contract_account.private_key]).rpc.get_table_rows({
            json: true,
            code: contract_account.name,
            scope: contract_account.name,
            table: 'timetable',
        }), rows = data['rows'];
        expect(rows[0].from).to.equal(sender_account.name);
        expect(rows[0].account).to.equal(contract_account.name);
        expect(rows[0].action).to.equal('dumb');
        expect(rows[0].period).to.equal(schedule_period);
        expect(rows[0].active).to.equal(1);
    })
});
