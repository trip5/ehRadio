#include "../../core/options.h"
#if DSP_MODEL!=DSP_DUMMY
#include <cstddef>
#include <iterator>
#include "../dspcore.h"
#include "pages.h"
#include "widgets.h"

void Pager::begin(){

}

void Pager::loop(){
  for(const auto& p: _pages)
    if(p->isActive()) p->loop();
}

Page& Pager::addPage(Page* page, bool setNow){
  _pages.push_back(page);
  if(setNow) setPage(page);
  return *page;
}

bool Pager::removePage(Page* page){
  page->setActive(false);
  dsp.clearDsp();
  auto i = std::find_if(_pages.begin(), _pages.end(), [&page](const Page* pn){ return page == pn; });
  if (i != _pages.end()){
    delete (*i);
    (*i) = nullptr;
    _pages.erase(i);
    return true;
  }
  return false;
  //return _pages.remove(page);
}

void Pager::setPage(Page* page, bool black){
  for(const auto& p: _pages) p->setActive(false);
  dsp.clearDsp(black);
  page->setActive(true);
}


/*******************************************************/

//Page::Page() : _widgets(LinkedList<Widget * >([](Widget * wd) { delete wd;})), _pages(LinkedList<Page*>([](Page* pg){ delete pg; })) {
//  _active = false;
//}

Page::~Page() {
  std::list<Widget*> draining;
  draining.swap(_widgets);
  for (const auto& w : draining) delete w;
  // what about deleting _pages ???
}

// The panel has ONE scroll slot (dsp.setScrollId()), and this is where it is handed out.  While it is free, the first
// widget that wants it - asked with the side-effect-free scrollWanted() - takes the turn, and the walk starts AFTER
// the widget served last, so the lines take turns instead of racing: no line can take two turns in a row while another
// wants one, the page's widget order no longer decides who scrolls, and a SCROLLDELAY_WEATHER of 0 (dspconf.h) stops
// meaning "always".  The owner then keeps the slot for its whole sweep and steps in its
// own loop() below.  A page with a single scrolling line is unaffected: it is the only candidate, so it is served at
// every handover.  See plans/scroll-turn-fairness.md.
void Page::loop() {
  if (!_active) return;
  if (!_widgets.empty() && dsp.getScrollId() == NULL) {
    const size_t n = _widgets.size();
    const size_t start = (_lastScroll < n) ? ((_lastScroll + 1) % n) : 0;
    auto it = _widgets.begin();
    std::advance(it, (std::ptrdiff_t)start);
    for (size_t k = 0; k < n; k++) {
      if (it == _widgets.end()) it = _widgets.begin();
      Widget* w = *it;
      if (w->scrollWanted()) {
        _lastScroll = (start + k) % n;
        w->scrollTurn();
        break;
      }
      ++it;
    }
  }
  for (const auto& w : _widgets) w->loop();
}

Widget& Page::addWidget(Widget* widget) {
  _widgets.push_back(widget);
  widget->setActive(_active, _active);
  return *widget;
}

Widget& Page::addWidgetFirst(Widget* widget) {
  _widgets.push_front(widget);
  widget->setActive(_active, _active);
  return *widget;
}

bool Page::removeWidget(Widget* widget){
  widget->setActive(false, _active);
  auto i = std::find_if(_widgets.begin(), _widgets.end(), [&widget](const Widget* wn){ return widget == wn; });
  if (i != _widgets.end()){
    delete (*i);
    (*i) = nullptr;
    _widgets.erase(i);
    return true;
  }
  return false;

  //return _widgets.remove(widget);
}

Page& Page::addPage(Page* page){
  _pages.push_back(page);
  return *page;
}

bool Page::removePage(Page* page){
  auto i = std::find_if(_pages.begin(), _pages.end(), [&page](const Page* pn){ return page == pn; });
  if (i != _pages.end()){
    delete (*i);
    (*i) = nullptr;
    _pages.erase(i);
    return true;
  }
  return false;
//  return _pages.remove(page);
}

void Page::setActive(bool act) {
  for(const auto& w: _widgets) w->setActive(act);
  for(const auto& p: _pages) p->setActive(act);
  _active = act;
}

bool Page::isActive() {
  return _active;
}

#endif // #if DSP_MODEL!=DSP_DUMMY
