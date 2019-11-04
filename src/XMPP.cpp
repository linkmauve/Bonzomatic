#include "XMPP.h"
#include "SXE.h"

#include <gloox/client.h>
#include <gloox/connectionlistener.h>
#include <gloox/disco.h>
#include <gloox/gloox.h>
#include <gloox/loghandler.h>
#include <gloox/message.h>
#include <gloox/messagesessionhandler.h>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

using namespace gloox;

class SxeHandler : public MessageSessionHandler
{
private:
  //ClientBase* parent;
  // TODO: allow more than one concurrent session.
  std::unique_ptr<MessageSession> session = nullptr;

public:
  bool setMessageSession(MessageSession* new_session)
  {
    if (session)
      return false;

    session.reset(new_session);
    printf("[33;1m[XMPP] Session with %s started[0m\n", session->target().full().c_str());
    return true;
  }

  void handleMessageSession(MessageSession* session) override
  {
    setMessageSession(session);
  }
};

class Bot : ConnectionListener,
            LogHandler,
            SxeHandler
{
private:
  std::unique_ptr<Client> client;
  bool connected = false;

public:
  Bot(JID jid, std::string password)
  {
    client = std::make_unique<Client>(jid, password);
    client->setSASLMechanisms(SaslMechPlain);
    client->setXmlLang("fr");
    client->disco()->setVersion("Bonzomatic", "1.0");
    client->disco()->setIdentity("client", "pc");
    client->registerConnectionListener(this);
    client->logInstance().registerLogHandler(LogLevelDebug,
                                             LogAreaXmlOutgoing | LogAreaXmlIncoming,
                                             this);
    client->registerMessageSessionHandler(this);
  }

  bool connect()
  {
    return client->connect(false);
  }

  void disconnect()
  {
    client->disconnect();
    connected = false;
  }

  bool isConnected()
  {
    return connected;
  }

  ConnectionError recv()
  {
    // Timeout every 16ms.
    return client->recv(16667);
  }

  void startSession(JID jid)
  {
    MessageSession* session = new MessageSession(client.get(), jid);
    if (!setMessageSession(session))
      return;

    Sxe sxe;
    sxe.id = "e";
    sxe.session = "851ba2";
    sxe.type = SxeType::Connect;
    StanzaExtensionList extensions;
    extensions.push_back(&sxe);

    session->send("", "", extensions);
  }

private:

  // For ConnectionListener.

  void onConnect() override
  {
    printf("[XMPP] Connected\n");
    connected = true;
  }

  void onDisconnect(ConnectionError e) override
  {
    printf("[XMPP] Disconnected\n");
    connected = false;
  }

  bool onTLSConnect(const CertInfo& info) override
  {
    printf("[XMPP] Connected with TLS\n");
    connected = false;
    return true;
  }

  // For LogHandler.

  void handleLog(LogLevel level, LogArea area, const std::string& message) override
  {
    const char* level_str;
    const char* area_str;
    if (level == LogLevelDebug) {
      level_str = "debug";
    } else if (level == LogLevelWarning) {
      level_str = "warn";
    } else if (level == LogLevelError) {
      level_str = "error";
    }
    if (area == LogAreaXmlOutgoing) {
      area_str = "-->";
    } else if (area == LogAreaXmlIncoming) {
      area_str = "<--";
    }
    printf("[XMPP] [%s] %s %s\n", level_str, area_str, message.c_str());
  }
};

static std::unique_ptr<std::thread> thread = nullptr;
static std::mutex mutex;
static std::queue<XMPP::Message> queue;

bool XMPP::Open(XMPP::Settings& settings)
{
  thread = std::make_unique<std::thread>([settings]
  {
    Bot bot(settings.jid, settings.password);
    bot.connect();
    while (true)
    {
      ConnectionError err = bot.recv();
      if (err != ConnNoError)
        break;

      // Only read the queue if we are connected.
      if (!bot.isConnected())
        continue;

      std::lock_guard<std::mutex> lock(mutex);
      while (!queue.empty())
      {
        Message msg = queue.front();
        if (msg == Message::Disconnect)
        {
          bot.disconnect();
        }
        else if (msg == Message::StartSession)
        {
          bot.startSession(JID(settings.recipient));
        }
        queue.pop();
      }
    }
  });

  pthread_setname_np(thread->native_handle(), "XMPP");

  return true;
}

bool XMPP::Close()
{
  if (!XMPP::SendMessage(Message::Disconnect))
    return false;

  thread->join();
  return true;
}

bool XMPP::SendMessage(Message msg)
{
  if (!thread)
    return false;

  std::lock_guard<std::mutex> lock(mutex);
  queue.push(msg);

  return true;
}
