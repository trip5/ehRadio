#ifndef pages_h
#define pages_h

#include <list>

class Widget;

class Page {
  protected:
    std::list<Widget*> _widgets;
    std::list<Page*> _pages;
    bool _active;
  public:
    //Page();
    ~Page();
    void loop();
    Widget& addWidget(Widget* widget);
    // Same, but in front of everything already on the page. A widget created later - when a layout switch brings one
    // in - would otherwise be appended and painted over what is already there.
    Widget& addWidgetFirst(Widget* widget);
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
