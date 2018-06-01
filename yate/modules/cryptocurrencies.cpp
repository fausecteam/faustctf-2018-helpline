/*
 * Cryptocurrency helpline. Service for FAUST CTF 2018,
 * copyright (c) 2018 Dominik Paulus
 *
 * Distributed under the terms of the GNU General Public License
 * version 2, see yate/COPYING for details
 */

#include <yatephone.h>
#include <set>
#include <string>
#include <map>
#include <cassert>
#include <sqlite3.h>
#include <cstring>
#include <algorithm>

using namespace TelEngine;
using std::map;
using std::string;
using std::memset;
using std::to_string;

namespace { // anonymous

enum CALLER_STATE {
	/*
	 * Make sure that state_support is the only state reachable by
	 * our exploit.
	 */
	STATE_SUPPORT = 65,
	STATE_SUP_ID = 97,
	STATE_INIT,
	STATE_HODL,
	STATE_BUY,
	STATE_PHONE,
	STATE_KNOWN,
	STATE_CREDIT,
	STATE_UPDATE,
	STATE_CALLBACK,
	STATE_LOT,
	STATE_CURRENCIES,
	STATE_BYE,
};

static Mutex s_mutex(true,"Crypto");
struct client_state {
	size_t offset;
	String dumbchan_id;
	char number[77];
	char secret[77];
	char buf[77];
	volatile char state;
	int uid;
};
static map<string, client_state> clients;
static sqlite3 *database;
static String soundpath;

class CryptoPlugin : public Plugin {
public:
	CryptoPlugin();
	~CryptoPlugin();
	virtual void initialize();
};

INIT_PLUGIN(CryptoPlugin);

class DtmfHandler : public MessageHandler {
public:
	DtmfHandler() : MessageHandler("chan.dtmf", 100, __plugin.name()) {
		// Empty
	}
	virtual bool received(Message &msg);
};

class MessageWatcher : public MessagePostHook {
public:
	MessageWatcher() {}
	virtual void dispatched(const Message &msg, bool handled);
};

class RouteHandler : public MessageHandler {
public:
	RouteHandler() : MessageHandler("call.route", 100, __plugin.name()) {
		// Empty
	}
	virtual bool received(Message &msg);
};

class HangupHandler : public MessageHandler {
public:
	HangupHandler() : MessageHandler("chan.hangup", 100, __plugin.name()) {
		// Empty
	}
	virtual bool received(Message &msg);
};

class NotifyHandler : public MessageHandler {
public:
	NotifyHandler() : MessageHandler("chan.notify", 100, __plugin.name()) {
		// Empty
	}
	virtual bool received(Message &msg);
};

#if 0
/*
 * Maps a character returned from a DTMF input
 * to an integer in the range [0;15]
 */
static unsigned int CharToIdx(char in) {
	int idx = 0;

	if(in >= '0' && in <= '9') {
		idx = in - '0';
	} else if(in >= 'A' && in <= 'D') {
		idx = in - 'A' + 10;
	} else if(in == '*') {
		idx = 14;
	} else
		idx = 15;

	return idx;
}
static char IdxToChar(int idx) {
	char ret;

	if(idx <= 9)
		ret = '0' + idx;
	else if(idx <= 13)
		ret = 'A' + idx - 10;
	else if(idx == 14)
		ret = '*';
	else
		ret = '#';

	return idx;
}
#endif

class DTMFSender : public Thread
{
private:
	String chan, text;
public:
		DTMFSender(String chan, String text)
			: Thread("DTMF sender thread", Thread::Normal),
			  chan(chan), text(text)
	{
		// empty
	}
	void run() {
		//Output("Sending DTMF '%s' to client '%s'", text.c_str(), chan.c_str());
		string s = text.c_str();
		for(char c : s) {
			char buf[2] = {c, 0};
			Message *m = new Message("chan.masquerade");
			m->addParam("message", "chan.dtmf");
			m->addParam("id", chan);
			m->addParam("text", buf);
			m->addParam("methods", "inband");
			m->addParam("duration", "2000");
			if(!Engine::dispatch(m))
				Output("Failed to send DTMF");
			usleep(200000);
		}
	}
};

static void send_dtmf(String chan, String text) {
	DTMFSender *t = new DTMFSender(chan, text);
	t->startup();
}

bool DtmfHandler::received(Message &msg) {
	String id = msg.getParam("id");
	String text = msg.getParam("text");
	String targetid = msg.getParam("targetid");
	if(id.null() || text.null() || targetid.null()) {
		return false;
	}
	//Output("Call ID: %s, target ID: %s, received DTMF: %s", id.c_str(), targetid.c_str(), text.c_str());

	struct client_state cur_state;
	{
		Lock lock(s_mutex);
		if(clients.find(id.c_str()) == clients.end())
			return false;
		cur_state = clients[id.c_str()];;
	}

	String newfile = "";
	bool autorepeat = false;
	switch(cur_state.state) {
	case STATE_INIT: // Init
		if(text[0] == '1') {
			cur_state.state = STATE_HODL;
			newfile = "hodl";
		} else {
			cur_state.state = STATE_BUY;
			newfile = "buy_instead";
		}
		break;
	case STATE_HODL:
		if(text[0] - '0' < 3) {
			cur_state.state = STATE_BUY;
			newfile = "notmuch_buy";
		} else {
			cur_state.state = STATE_PHONE;
			cur_state.offset = 0;
			newfile = "phone_1";
		}
		break;
	case STATE_BUY:
		if(text[0] == '1') {
			cur_state.state = STATE_PHONE;
			cur_state.offset = 0;
			newfile = "phone_2";
		} else {
			cur_state.state = STATE_BYE;
			newfile = "bye_2";
		}
		break;
	case STATE_PHONE:
		if(text[0] == '#') {
			Lock lock(s_mutex);
			cur_state.buf[cur_state.offset] = 0;
			snprintf(cur_state.number, sizeof(cur_state.number), "%s", cur_state.buf);
			/* Check whether that user has already registered before. */
			sqlite3_stmt *stmt;
			int ret = sqlite3_prepare_v2(database, "SELECT creditcard FROM cryptohelp WHERE number LIKE ?", -1, &stmt, NULL);
			if(ret != SQLITE_OK) {
				Output("[Cryptocurrencies] Unable to compile SQLite statement: %s", sqlite3_errmsg(database));
				cur_state.state = STATE_BYE;
				newfile = "error";
				break;
			}
			if(sqlite3_bind_text(stmt, 1, cur_state.number, -1, NULL) != SQLITE_OK) {
				Output("[Cryptocurrencies] Unable to bind value to SQLite prepared statement: %s", sqlite3_errmsg(database));
				sqlite3_finalize(stmt);
				cur_state.state = STATE_BYE;
				newfile = "error";
				break;
			}
			ret = sqlite3_step(stmt);
			if(ret == SQLITE_ROW) {
				/* User already exists. */
				const unsigned char *creditcard = sqlite3_column_text(stmt, 0);
				cur_state.state = STATE_KNOWN;
				snprintf(cur_state.secret, sizeof(cur_state.secret), "%s", creditcard);
				newfile = "alreadyknown";
			} else {
				cur_state.state = STATE_CREDIT;
				cur_state.offset = 0;
				newfile = "billed";
			}
			sqlite3_finalize(stmt);
			break;
		}

		if(cur_state.offset < sizeof(cur_state.buf))
			cur_state.buf[cur_state.offset++] = text[0];

		break;
	case STATE_KNOWN:
		if(text[0] == '1') {
			cur_state.state = STATE_BYE;
			if(random() & 1)
				newfile = "loop_1";
			else
				newfile = "loop_2";
			autorepeat = true;
		} else if(text[0] == '2') {
			cur_state.state = STATE_KNOWN;
			newfile = "";
			send_dtmf(targetid, cur_state.secret);
		} else if(text[0] == '3') {
			cur_state.state = STATE_UPDATE;
			cur_state.offset = 0;
			newfile = "update";
		} else {
			cur_state.state = STATE_KNOWN;
			newfile = "alreadyknown";
		}
		break;
	case STATE_UPDATE:
		if(text[0] == '#') {
			cur_state.buf[cur_state.offset] = 0;
			snprintf(cur_state.secret, sizeof(cur_state.secret), "%s", cur_state.buf);
			Lock lock(s_mutex);
			/* Insert user into database */
			sqlite3_stmt *stmt;
			int ret = sqlite3_prepare_v2(database, "UPDATE cryptohelp SET creditcard = ? WHERE number = ?", -1, &stmt, NULL);
			if(ret != SQLITE_OK) {
				Output("[Cryptocurrencies] Unable to compile SQLite statement: %s", sqlite3_errmsg(database));
				return true;
			}
			if(sqlite3_bind_text(stmt, 2, cur_state.number, -1, NULL) != SQLITE_OK ||
					sqlite3_bind_text(stmt, 1, cur_state.secret, -1, NULL) != SQLITE_OK) {
				cur_state.state = STATE_BYE;
				newfile = "error";
				Output("[Cryptocurrencies] Unable to bind value to SQLite prepared statement: %s", sqlite3_errmsg(database));
				sqlite3_finalize(stmt);
				break;
			}
			ret = sqlite3_step(stmt);
			if(ret != SQLITE_DONE) {
				Output("[Cryptocurrencies] Unable to execute SQLite statement: %s", sqlite3_errmsg(database));
				sqlite3_finalize(stmt);
				cur_state.state = STATE_BYE;
				newfile = "error";
				break;
			}
			sqlite3_finalize(stmt);
			cur_state.state = STATE_KNOWN;
			cur_state.offset = 0;
			newfile = "alreadyknown";
			break;
		}

		if(cur_state.offset < sizeof(cur_state.buf))
			cur_state.buf[cur_state.offset++] = text[0];

		break;
	case STATE_CREDIT:
		if(text[0] == '#') {
			cur_state.buf[cur_state.offset] = 0;
			snprintf(cur_state.secret, sizeof(cur_state.secret), "%s", cur_state.buf);
			Lock lock(s_mutex);
			/* Insert user into database */
			sqlite3_stmt *stmt;
			int ret = sqlite3_prepare_v2(database, "INSERT INTO cryptohelp (number, creditcard) VALUES (?, ?)", -1, &stmt, NULL);
			if(ret != SQLITE_OK) {
				Output("[Cryptocurrencies] Unable to compile SQLite statement: %s", sqlite3_errmsg(database));
				return true;
			}
			if(sqlite3_bind_text(stmt, 1, cur_state.number, -1, NULL) != SQLITE_OK ||
					sqlite3_bind_text(stmt, 2, cur_state.secret, -1, NULL) != SQLITE_OK) {
				cur_state.state = STATE_BYE;
				newfile = "error";
				Output("[Cryptocurrencies] Unable to bind value to SQLite prepared statement: %s", sqlite3_errmsg(database));
				sqlite3_finalize(stmt);
				break;
			}
			ret = sqlite3_step(stmt);
			if(ret != SQLITE_DONE) {
				Output("[Cryptocurrencies] Unable to execute SQLite statement: %s", sqlite3_errmsg(database));
				sqlite3_finalize(stmt);
				cur_state.state = STATE_BYE;
				newfile = "error";
				break;
			}
			sqlite3_finalize(stmt);
			string uid = to_string(sqlite3_last_insert_rowid(database)) + "#";
			//Output("Sending UID: %s", uid.c_str());
			send_dtmf(targetid, uid.c_str());
			cur_state.state = STATE_CALLBACK;
			cur_state.offset = 0;
			newfile = "callback";
			break;
		}

		if(cur_state.offset <= sizeof(cur_state.buf))
			cur_state.buf[cur_state.offset++] = text[0];

		break;
	case STATE_CALLBACK:
		if(text[0] == '2') {
			cur_state.state = STATE_BYE;
			if(random() & 1)
				newfile = "loop_1";
			else
				newfile = "loop_2";
			autorepeat = true;
		} else {
			cur_state.state = STATE_BYE;
			newfile = "bye_1";
		}
		break;
	case STATE_BYE:
		return true;
	case STATE_SUPPORT:
		//Output("In support state!");
		newfile = "sup_init";
		if(text[0] == '1') {
			/* Query current user count */
			Lock lock(s_mutex);
			sqlite3_stmt *stmt;
			int ret = sqlite3_prepare_v2(database, "SELECT MAX(uid) FROM cryptohelp", -1, &stmt, NULL);
			if(ret != SQLITE_OK) {
				Output("[Cryptocurrencies] Unable to compile SQLite statement: %s", sqlite3_errmsg(database));
				cur_state.state = STATE_BYE;
				newfile = "error";
				break;
			}
			ret = sqlite3_step(stmt);
			if(ret == SQLITE_ROW) {
				uint64_t count = sqlite3_column_int64(stmt, 1);
				//Output("[Cryptocurrencies] User %s queried for user count. Current count: %llu", id.c_str(), (long long unsigned int) count);

				string out = to_string(count);

				send_dtmf(targetid, out.c_str());
			} else {
				cur_state.state = STATE_BYE;
				newfile = "error";
			}
			sqlite3_finalize(stmt);
			cur_state.state = STATE_SUPPORT;
		} else if(text[0] == '2') {
			/* Query a specific user */
			cur_state.state = STATE_SUP_ID;
			cur_state.offset = 0;
			newfile = "sup_data";
		}
		break;
	case STATE_SUP_ID:
		if(text[0] == '#') {
			cur_state.buf[cur_state.offset] = 0;
			//Output("[Cryptocurrencies] User %s queried for user details on user %s.", id.c_str(), cur_state.buf);
			Lock lock(s_mutex);
			sqlite3_stmt *stmt;
			int ret = sqlite3_prepare_v2(database, "SELECT creditcard FROM cryptohelp WHERE uid LIKE ?", -1, &stmt, NULL);
			if(ret != SQLITE_OK) {
				Output("[Cryptocurrencies] Unable to compile SQLite statement: %s", sqlite3_errmsg(database));
				cur_state.state = STATE_BYE;
				newfile = "error";
				break;
			}
			if(sqlite3_bind_text(stmt, 1, cur_state.buf, -1, NULL) != SQLITE_OK) {
				Output("[Cryptocurrencies] Unable to bind value to SQLite prepared statement: %s", sqlite3_errmsg(database));
				sqlite3_finalize(stmt);
				cur_state.state = STATE_BYE;
				newfile = "error";
				break;
			}
			ret = sqlite3_step(stmt);
			if(ret != SQLITE_ROW) {
				cur_state.state = STATE_BYE;
				newfile = "error";
				break;
			}
			const unsigned char *creditcard = sqlite3_column_text(stmt, 0);
			//Output("[Cryptocurrencies] Credit card number is: %s", creditcard);

			send_dtmf(targetid, (char*) creditcard);

			sqlite3_finalize(stmt);

			cur_state.state = STATE_SUPPORT;
			newfile = "";
		} else {
			if(cur_state.offset < sizeof(cur_state.buf))
				cur_state.buf[cur_state.offset++] = text[0];
		}
		break;
	default:
		return true;
	}

	if(newfile != "") {
		Message *m = new Message("chan.masquerade");
		m->addParam("message", "chan.attach");
		m->addParam("id", targetid.c_str());
		m->addParam("source", ("wave/play/" + soundpath + newfile + ".mulaw").c_str());
		m->addParam("autorepeat", autorepeat ? "true" : "false");
		if(cur_state.state == STATE_BYE)
			m->addParam("notify", "cryptocurrencies/" + id);
		if(!Engine::enqueue(m)) {
			Output("Failed to attach channel to wave/play");
			return true;
		}
	}

	{
		Lock lock(s_mutex);
		clients[id.c_str()] = cur_state;
	}

	return true;
}

void MessageWatcher::dispatched(const Message &msg, bool handled) {
	if(msg != "call.execute")
		return;
	//Output("MessageWatcher::dispatched()");
	if(!handled) {
		Output("[cryptocurrencies] Warning: Catched call.execute-message that wasn't handled. Something is wrong.");
		return;
	}

	String targetid = msg.getParam("targetid");
	String id = msg.getParam("id");
#if 0
	Output("targetid: %s", targetid.c_str());
	Output("id: %s", id.c_str());
#endif

	if(targetid.null() || id.null())
		return;

	{
		Lock lock(s_mutex);
		if(clients.find(id.c_str()) == clients.end())
			return;
		clients[id.c_str()].dumbchan_id = targetid;
	}

	//Output("[cryptocurrencies] Messagewatcher called for one of my connections");

	Message *m = new Message("call.answered");
	m->addParam("targetid", id.c_str());
	m->addParam("id", targetid.c_str());
	if(!Engine::dispatch(m)) {
		Output("Failed to answer call?!");
		return;
	}

	m = new Message("chan.masquerade");
	m->addParam("message", "chan.attach");
	m->addParam("id", targetid.c_str());
	m->addParam("source", "wave/play/" + soundpath + "welcome.mulaw");
	if(!Engine::enqueue(m)) {
		Output("Failed to attach to welcome message");
		return;
	}

	//Output("[cryptocurrencies] Call %s has been answered and is attached to welcome message", id.c_str());
}

bool RouteHandler::received(Message &msg) {
#if 0
	String called(msg.getValue("called"));
	if(called.null() || called != "cryptohelp")
		return false;
#endif

	String id(msg.getValue("id"));
	//Output("Received call to cryptohelp@. Routing call to dumb/");
	//Output("Call ID is: %s", id.c_str());
	msg.addParam("tonedetect_in", "tone/*");
	msg.retValue() = "dumb/";

	{
		Lock lock(s_mutex);
		struct client_state s;
		memset(&s, 0, sizeof(s));
		s.state = STATE_INIT;
		s.uid = -1;
		clients[id.c_str()] = s;
	}

	return true;
}

bool HangupHandler::received(Message &msg) {
	String id(msg.getValue("id"));
	if(id.null())
		return false;

	{
		Lock lock(s_mutex);
		clients.erase(id.c_str());
	}

	//Output("Erased client %s", id.c_str());

	return true;
}

bool NotifyHandler::received(Message &msg) {
	String targetid(msg.getValue("targetid"));
	if(targetid.null())
		return false;

	/*
	Output("Notify: %s", targetid.c_str());
	Output("Starts: %d", targetid.startsWith("cryptocurrencies/", false, false));
	Output("Starts: %d", targetid.startsWith("cryptocurrencies/", false, false));
	Output("startSkip: %d", targetid.startSkip("cryptocurrencies/", false, false));
	Output("Notify: %s", targetid.c_str());
	*/

	if(!targetid.startSkip("cryptocurrencies/", false, false))
		return false;

	//Output("Channel ID is %s", targetid.c_str());

	client_state state;
	{
		Lock lock(s_mutex);
		auto s = clients.find(targetid.c_str());
		if(s == clients.end())
			return true;
		state = clients[targetid.c_str()];
	}

	if(state.state == STATE_BYE) {
		//Output("Disconnecting client %s", targetid.c_str());
		Message *m = new Message("call.drop");
		//m->addParam("message", "chan.hangup");
		m->addParam("id", state.dumbchan_id);
		//m->addParam("targetid", targetid);
		if(!Engine::dispatch(m)) {
			Output("Failed to drop client");
			return false;
		}
	}

	return true;
}

CryptoPlugin::CryptoPlugin()
	: Plugin("crypto") {
	Output("Loaded module Crypto(currencies)");
}

CryptoPlugin::~CryptoPlugin() {
	Output("Unloading module cryptocurrencies: Not implemented!");
}

void CryptoPlugin::initialize() {
	Output("Initializing module Crypto");
	if(!database) {
		int ret = sqlite3_open("/srv/helpline/data/cryptocurrencies.sqlite", &database);
		if(ret != SQLITE_OK) {
			Output("Unable to initialize SQLite database for module cryptocurrencies: %s", sqlite3_errmsg(database));
			assert(false);
		}
		/* Let's be friendly to teams killing their databases :-) */
		sqlite3_stmt *stmt;
		ret = sqlite3_prepare_v2(database, "CREATE TABLE IF NOT EXISTS 'cryptohelp' (uid INTEGER PRIMARY KEY AUTOINCREMENT, number UNIQUE, creditcard)", -1, &stmt, NULL);
		if(ret != SQLITE_OK) {
			Output("[Cryptocurrencies] Unable to compile SQLite statement: %s", sqlite3_errmsg(database));
			assert(false);
		}
		ret = sqlite3_step(stmt);
		if(ret != SQLITE_DONE) {
			Output("[Cryptocurrencies] Unable to initialize database schema: %s", sqlite3_errmsg(database));
			assert(false);
		}
		sqlite3_finalize(stmt);
	}
	soundpath << Engine::sharedPath() << Engine::pathSeparator() << "sounds" << Engine::pathSeparator()
			  << "helpline" << Engine::pathSeparator();
	Engine::self()->setHook(new MessageWatcher);
	Engine::install(new RouteHandler);
	Engine::install(new DtmfHandler);
	Engine::install(new HangupHandler);
	Engine::install(new NotifyHandler);
}

} // anonymous namespace
