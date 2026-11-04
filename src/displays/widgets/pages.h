#ifndef pages_h
#define pages_h

#include <list>

class Widget;

class Page {
  protected:
    std::list<Widget*> _widgets;
    std::list<Page*> _pages;
    bool _active;
    // The widget INDEX served the panel's scroll slot most recently, so the next turn starts after it.  `(size_t)-1`
    // means "none yet", and an index the list has since outgrown (a widget was removed) simply starts the walk at the
    // head again.  This is what turns the slot from a race into a queue - see plans/scroll-turn-fairness.md.
    size_t _lastScroll = (size_t)-1;
  public:
    //Page();
    ~Page();
    void loop();
    Widget& addWidget(Widget* widget);
    // Same, but in front of everything already on the page. A widget created later - when a layout switch brings one
    // in - would otherwise be appended and painted over what is already there.
    Widget& addWidgetFirst(Widget* widget);
    // Detaches the widget from the page and DELETES it, like removePage() below.  The caller must only null its
    // own pointer: pairing this with a delete is a double free, which is what a reboot on a layout switch turned
    // out to be.
    bool removeWidget(Widget* widget);
    Page& addPage(Page* page);
    bool removePage(Page* page);
    void setActive(bool act);
    bool isActive();
};

class Pager{
  public:
    //Pager() : _pages(std::list<Page*>([](Page* pg){ delete pg; })) {}
    void begin();
    void loop();
    Page& addPage(Page* page, bool setNow = false);
    bool removePage(Page* page);
    void setPage(Page* page, bool black=false);
  private:
    std::list<Page*> _pages;
    
};


#endif
