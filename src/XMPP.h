#include <string>

namespace XMPP
{
  enum class Message
  {
    Disconnect,
    Connect,
    StartSession,
  };

  struct Settings
  {
    std::string jid;
    std::string password;
    std::string recipient;
    bool host;
  };

  bool Open(XMPP::Settings& settings);
  bool Close();
  bool SendMessage(Message);
}
