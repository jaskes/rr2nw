#include <cstdlib>
#include <iostream>

#include "kernel/h/context.h"
#include "message/pubmsg.h"
#include "obase/publish/publish.h"

namespace {

int Fail(const char* message) {
  std::cerr << "legacy-publisher-lifecycle-smoke: " << message << '\n';
  return EXIT_FAILURE;
}

class ProbeObject final : public KR_Object {
 public:
  int receiveEvent(KR_Event&) override { return 1; }
  bool shouldDump() override { return false; }
};

void SetSubscription(KR_Event& event,
                     KR_EventLabel label,
                     KR_ObjectID author,
                     KR_ObjectID subscriber) {
  event.label = EVT_SUBSCRIPT_TO_EVENT;
  event.data.open(EDO_WRITE)
      .putInt(label)
      .putObjectID(author)
      .putObjectID(subscriber)
      .putInt(FALSE)
      .close();
}

}  // namespace

int main() {
  for (int iteration = 0; iteration < 64; ++iteration) {
    Publisher publisher(8, 16);
    if (publisher.m_eventDescrQnty != 8 || publisher.m_eventFreeList != 0 ||
        publisher.m_subscriberQnty != 16 ||
        publisher.m_subscriberFreeList != 0) {
      return Fail("Publisher allocation state diverged");
    }
  }

  SimulationContext context(16, 16);
  Publisher publisher(8, 16);
  ProbeObject author;
  ProbeObject subscriber;
  context.addObject("Publisher", &publisher);
  publisher.addNotify();
  const KR_ObjectID authorId = context.addObject("Author", &author);
  const KR_ObjectID subscriberId =
      context.addObject("Subscriber", &subscriber);

  KR_Event first;
  KR_Event second;
  SetSubscription(first, 7001, authorId, subscriberId);
  SetSubscription(second, 7002, authorId, subscriberId);
  if (!publisher.receiveEvent(first) || !publisher.receiveEvent(second) ||
      publisher.m_subscriberFreeList != 2) {
    return Fail("Publisher did not retain two event subscriptions");
  }

  KR_Event fullUnsubscribe;
  fullUnsubscribe.label = EVT_FULL_UNSUBSCRIPT_TO_AUTHOR;
  fullUnsubscribe.data.open(EDO_WRITE)
      .putObjectID(authorId)
      .putObjectID(subscriberId)
      .close();
  if (!publisher.receiveEvent(fullUnsubscribe) ||
      publisher.m_subscriberFreeList != 1) {
    return Fail("full unsubscribe did not release every event subscription");
  }

  KR_Event repeatedFirst;
  KR_Event repeatedSecond;
  SetSubscription(repeatedFirst, 7001, authorId, subscriberId);
  SetSubscription(repeatedSecond, 7002, authorId, subscriberId);
  if (!publisher.receiveEvent(repeatedFirst) ||
      !publisher.receiveEvent(repeatedSecond) ||
      publisher.m_subscriberFreeList != 2) {
    return Fail("released Publisher subscription slots were not reusable");
  }

  std::cout << "legacy-publisher-lifecycle-smoke: OK\n";
  return EXIT_SUCCESS;
}
