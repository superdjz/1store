#include "redis/commands/get.hh"
#include "redis/commands/unexpected.hh"
#include "redis/reply_builder.hh"
#include "redis/request.hh"
#include "redis/reply.hh"
#include "timeout_config.hh"
#include "service/client_state.hh"
#include "service/storage_proxy.hh"
#include "db/system_keyspace.hh"
#include "partition_slice_builder.hh"
#include "gc_clock.hh"
#include "dht/i_partitioner.hh"
#include "log.hh"
namespace redis {
namespace commands {
static logging::logger log("command_get");
shared_ptr<abstract_command> get::prepare(request&& req)
{
    if (req._args_count < 1) {
        return unexpected::prepare(std::move(req._command), std::move(bytes { msg_syntax_err }) );
    }
    return make_shared<get>(std::move(req._command), std::move(req._args[0]));
}

future<reply> get::execute(service::storage_proxy& proxy, db::consistency_level cl, db::timeout_clock::time_point now, const timeout_config& tc, service::client_state& cs)
{
    auto& db = proxy.get_db().local();
    auto schema = db.find_schema(db::system_keyspace::redis::NAME, db::system_keyspace::redis::SIMPLE_OBJECTS);
    auto timeout = now + tc.read_timeout;
    auto fetched = prefetch_partition_helper::prefetch_simple(proxy, schema, _key, cl, timeout, cs);
    return fetched.then([this, &proxy, cl, timeout, &cs, schema] (auto pd) {
        if (pd && pd->fetched()) {
            return reply_builder::build<message_tag>(std::move(pd->_data));
        }
        return reply_builder::build<null_message_tag>();
    });
}
}
}