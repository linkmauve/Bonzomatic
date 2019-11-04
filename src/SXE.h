#include <gloox/stanzaextension.h>
#include <memory>
#include <string>
#include <vector>

static const std::string XMLNS_SXE = "urn:xmpp:sxe:0";

enum class SxeType
{
  Invalid,
  Connect,
  StateOffer,
  AcceptState,
  RefuseState,
  State,
};

enum class StateChangeType
{
  DocumentBegin,
  DocumentEnd,
  New,
  Remove,
  Set,
};

struct DocumentBegin
{
  const char* prolog;
};

struct DocumentEnd
{
  const char* last_sender;
  const char* last_id;
};

struct New
{
  const char* rid;
  const char* type;
  const char* name;
  const char* ns;
  const char* parent;
  const char* chdata;
};

struct Remove
{
  const char* target;
};

struct Set
{
  const char* target;
  const char* version;
  const char* parent;
  const char* name;
  const char* ns;
  const char* chdata;
};

struct StateChange
{
  StateChangeType type;
  union
  {
    DocumentBegin document_begin;
    DocumentEnd document_end;
    New new_;
    Remove remove;
    Set set;
  };
};

class Sxe : public gloox::StanzaExtension
{
public:
  std::string id = "UNSET";
  std::string session = "UNSET";
  SxeType type = SxeType::Invalid;

private:
  std::vector<std::string> state_offer_xmlns;
  std::vector<StateChange> state_changes;

public:
  Sxe(const gloox::Tag* tag = 0);
  gloox::Tag* tag() const;

  gloox::StanzaExtension* newInstance(const gloox::Tag* tag) const
  {
    return new Sxe(tag);
  }

  gloox::StanzaExtension* clone() const
  {
    return new Sxe(*this);
  }

  const std::string& filterString() const override
  {
    static const std::string filter = "/message/sxe[@xmlns='" + XMLNS_SXE + "']";
    return filter;
  }
};
