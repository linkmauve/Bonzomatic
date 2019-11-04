#include "SXE.h"

#include <gloox/stanzaextension.h>
#include <gloox/tag.h>
#include <memory>
#include <string>

using namespace gloox;

Sxe::Sxe(const Tag* tag)
  : StanzaExtension(0xffff)
{
  if (!tag || tag->name() != "sxe" || tag->xmlns() != XMLNS_SXE)
    return;

  id = tag->findAttribute("id");
  session = tag->findAttribute("session");
  for (const Tag* child : tag->children())
  {
    if (child->xmlns() != XMLNS_SXE)
      break;

    if (child->name() == "connect")
      type = SxeType::Connect;
    else if (child->name() == "state-offer")
    {
      for (const Tag* description : child->children())
      {
        if (description->name() != "description")
          return;
        state_offer_xmlns.push_back(description->xmlns());
      }
      type = SxeType::StateOffer;
    }
    else if (child->name() == "accept-state")
      type = SxeType::AcceptState;
    else if (child->name() == "refuse-state")
      type = SxeType::RefuseState;
    else if (child->name() == "state")
    {
      for (const Tag* child2 : child->children())
      {
        if (child2->xmlns() != XMLNS_SXE)
          return;

        StateChange change;
        if (child2->name() == "document-begin")
        {
          change.type = StateChangeType::DocumentBegin;
          change.document_begin.prolog = child2->findAttribute("prolog").c_str();
        }
        else if (child2->name() == "document-end")
        {
          change.type = StateChangeType::DocumentEnd;
          change.document_end.last_sender = child2->findAttribute("last-sender").c_str();
          change.document_end.last_id = child2->findAttribute("last-id").c_str();
        }
        else if (child2->name() == "new")
        {
          change.type = StateChangeType::New;
          change.new_.rid = child2->findAttribute("rid").c_str();
          change.new_.type = child2->findAttribute("type").c_str();
          change.new_.name = child2->findAttribute("name").c_str();
          change.new_.ns = child2->findAttribute("ns").c_str();
          change.new_.chdata = child2->findAttribute("chdata").c_str();
        }
        else if (child2->name() == "remove")
        {
          change.type = StateChangeType::Remove;
          change.remove.target = child2->findAttribute("target").c_str();
        }
        else if (child2->name() == "set")
        {
          change.type = StateChangeType::Set;
          change.set.target = child2->findAttribute("target").c_str();
          change.set.version = child2->findAttribute("version").c_str();
          change.set.name = child2->findAttribute("name").c_str();
          change.set.ns = child2->findAttribute("ns").c_str();
          change.set.chdata = child2->findAttribute("chdata").c_str();
        }
        else
        {
          return;
        }
        state_changes.push_back(change);
      }
      type = SxeType::State;
    }
  }
}

Tag* Sxe::tag() const
{
  if (type == SxeType::Invalid)
    return nullptr;

  Tag* t = new Tag("sxe");
  t->setXmlns(XMLNS_SXE);
  t->addAttribute("id", id);
  t->addAttribute("session", session);

  if (type == SxeType::Connect)
    new Tag(t, "connect");
  else if (type == SxeType::AcceptState)
    new Tag(t, "accept-state");
  else if (type == SxeType::RefuseState)
    new Tag(t, "refuse-state");
  else if (type == SxeType::StateOffer)
  {
    Tag* child = new Tag(t, "state-offer");
    child->setXmlns(XMLNS_SXE);
    for (const std::string& xmlns : state_offer_xmlns)
    {
      Tag* description = new Tag(child, "description");
      description->setXmlns(xmlns);
    }
  }
  else if (type == SxeType::State)
  {
    Tag* state = new Tag(t, "state");
    state->setXmlns(XMLNS_SXE);
    for (const auto& change : state_changes)
    {
      Tag* child;
      if (change.type == StateChangeType::DocumentBegin)
      {
        child = new Tag(state, "document-begin");
        child->addAttribute("prolog", change.document_begin.prolog);
      }
      else if (change.type == StateChangeType::DocumentEnd)
      {
        child = new Tag(state, "document-end");
        child->addAttribute("last-sender", change.document_end.last_sender);
        child->addAttribute("last-id", change.document_end.last_id);
      }
      else if (change.type == StateChangeType::New)
      {
        child = new Tag(state, "new");
        child->addAttribute("rid", change.new_.rid);
        child->addAttribute("type", change.new_.type);
        child->addAttribute("name", change.new_.name);
        child->addAttribute("ns", change.new_.ns);
        child->addAttribute("chdata", change.new_.chdata);
      }
      else if (change.type == StateChangeType::Remove)
      {
        child = new Tag(state, "remove");
        child->addAttribute("target", change.remove.target);
      }
      else if (change.type == StateChangeType::Set)
      {
        child = new Tag(state, "set");
        child->addAttribute("target", change.set.target);
        child->addAttribute("version", change.set.version);
        child->addAttribute("name", change.set.name);
        child->addAttribute("ns", change.set.ns);
        child->addAttribute("chdata", change.set.chdata);
      }
      else
      {
        return nullptr;
      }
    }
  }

  return t;
}
