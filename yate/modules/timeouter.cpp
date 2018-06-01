/*
 * Helper module for FAUST CTF 2018. Automatically drops calls
 * after a timeout of 6 minutes to make sure that running YATE
 * instances are not clogged with idle connections during the
 * CTF-contest.
 * Copyright (c) 2018 Dominik Paulus
 *
 * Distributed under the terms of the GNU General Public License
 * version 2, see yate/COPYING for details
 */

#include <yatephone.h>
#include <set>
#include <string>
#include <set>
#include <cassert>
#include <sqlite3.h>
#include <cstring>
#include <algorithm>
#include <utility>

using namespace TelEngine;
using std::set;
using std::pair;
using std::string;

namespace { // anonymous

static Mutex s_mutex(true,"Crypto");
// time_t (connection date) -> channel-ID
static set<pair<time_t, String>> connections;

class TimeouterPlugin : public Plugin {
public:
	TimeouterPlugin();
	~TimeouterPlugin();
	virtual void initialize();
};

INIT_PLUGIN(TimeouterPlugin);

class MessageWatcher : public MessagePostHook {
public:
	MessageWatcher() {}
	virtual void dispatched(const Message &msg, bool handled);
};

void MessageWatcher::dispatched(const Message &msg, bool handled) {
	if(msg != "call.execute")
		return;

	String targetid = msg.getParam("targetid");
	String id = msg.getParam("id");

	if(targetid.null() || id.null())
		return;

	{
		Lock lock(s_mutex);
		connections.insert({time(NULL), id});
		while(connections.size() &&
			  time(NULL) - connections.begin()->first > 360) {
			auto c = *connections.begin();
			connections.erase(connections.begin());
			Message *m = new Message("call.drop");
			m->addParam("id", c.second);
			if(!Engine::enqueue(m)) {
				Output("Failed to drop client");
				return;
			}
			Output("Dropping client %s after timeout", id.c_str());
		}
	}
}

TimeouterPlugin::TimeouterPlugin()
	: Plugin("crypto") {
	Output("Loaded module Timeouter");
}

TimeouterPlugin::~TimeouterPlugin() {
	Output("Unloading module Timeouter");
}

void TimeouterPlugin::initialize() {
	Output("Initializing module Timeouter");
	Engine::self()->setHook(new MessageWatcher);
}

} // anonymous namespace
