#include "prose_ui.hpp"

namespace prose {
namespace {
constexpr int libraryTop = 246;
constexpr int libraryBottom = 752;
constexpr int listTop = 636;
constexpr int rowHeight = 58;
constexpr int fontTop = 196;
constexpr int fontRow = 49;
constexpr int themeTop = 627;
constexpr const char* paperNames[]={"Blank","Lined","Dot grid","Checklist"};
constexpr const char* keys[]={"QWERTYUIOP","ASDFGHJKL","ZXCVBNM"};
constexpr int fontCount = 4;

struct FontChoice {
    const char* label;
    Font face;
    int size;
};
constexpr FontChoice readingFonts[] = {
    {"Serif",Font::ReaderSerif,22},
    {"Sans serif",Font::ReaderSans,22},
    {"Monospace",Font::ReaderMono,21},
    {"OpenDyslexic",Font::OpenDyslexic,18},
};
bool validFont(ReadingFont font) { return int(font)>=0 && int(font)<fontCount; }
int magnitude(int value) { return value < 0 ? -value : value; }
int contentBottom(int rows) { return listTop + rows * rowHeight + 12; }

bool within(int x, int y, int left, int top, int w, int h) {
    return x >= left && y >= top && x < left + w && y < top + h;
}

void outline(const Display& d, int x, int y, int w, int h) {
    d.line(x, y, x+w, y, 1);
    d.line(x+w, y, x+w, y+h, 1);
    d.line(x+w, y+h, x, y+h, 1);
    d.line(x, y+h, x, y, 1);
}

void chip(const Display& d, int x, int y, int w, const char* label, bool active) {
    if (active) d.fill(x, y, w, 34, true);
    else outline(d, x, y, w, 34);
    d.text(x+13, y+22, 13, Font::Sans, label, !active);
}

void decimal(char*& p, int number) {
    char digits[10];
    int count=0;
    do {
        digits[count++] = char('0'+number%10);
        number /= 10;
    } while (number > 0 && count < 10);
    while (count > 0) *p++ = digits[--count];
}

void batteryLabel(char* label, int percent) {
    if (percent<0) {
        *label++='-';
        *label++='-';
    } else {
        decimal(label,percent);
        *label++='%';
    }
    *label='\0';
}

void batteryBadge(const Display& d, int percent) {
    outline(d,224,23,26,11);
    d.fill(251,26,2,6,true);
    if (percent>=0) {
        const int filled=(22*percent+99)/100;
        if (filled>0) d.fill(226,25,filled,8,true);
    } else {
        d.line(233,28,241,28,1);
    }
    char label[6];
    batteryLabel(label,percent);
    d.text(261,34,12,Font::Label,label,true);
}

void header(const Display& d, const char* label, Icon corner, int battery) {
    d.icon(Icon::Logo, 30, 18, 22, true);
    d.text(61, 35, 13, Font::Label, "DSH / PROSE", true);
    d.text(corner==Icon::Home ? 306 : 358, 35, 12, Font::Label, label, true);
    if (corner==Icon::Home) d.icon(Icon::Settings,385,18,22,true);
    if (corner!=Icon::Logo) d.icon(corner,428,18,22,true);
    batteryBadge(d,battery);
    d.line(28, 53, 452, 53, 1);
}

void bookPreview(const Display& d, int x, int top, const LibraryItems& items) {
    d.fill(x, top, 204, 233, true);
    d.line(x+15, top+16, x+188, top+16, 1);
    d.text(x+16, top+39, 12, Font::Label, "READING", false);
    d.text(x+16, top+131, 36, Font::Serif, items.bookCoverLine1, false);
    d.text(x+16, top+170, 36, Font::Serif, items.bookCoverLine2, false);
    d.text(x+16, top+211, 12, Font::Label, "BOOK  /  TXT", false);
}

void notebookPreview(const Display& d, int x, int top) {
    outline(d, x, top, 203, 232);
    for (int y = top+16; y < top+224; y += 12)
        for (int offset = 12; offset < 195; offset += 12)
            d.fill(x+offset,y,1,1,true);
    d.text(x+17, top+39, 12, Font::Label, "PERSONAL", true);
    const int points[][2]={{19,162},{45,149},{68,157},{105,101},{129,122},{155,88},{184,113}};
    for (int i=1;i<7;++i)
        d.line(x+points[i-1][0],top+points[i-1][1],x+points[i][0],top+points[i][1],2);
    for (int i=0;i<7;++i)
        d.line(x+85+i*7,top+137-i*7,x+107+i*7,top+169-i*3,1);
    d.text(x+17, top+212, 15, Font::Serif, "Things to keep.", true);
}
} // namespace

int readingSize(Preferences p) {
    return readingFonts[validFont(p.font) ? int(p.font) : 0].size+(p.scale-1)*3;
}
Font readingFace(ReadingFont font) { return readingFonts[validFont(font) ? int(font) : 0].face; }
int lineStep(Preferences p) {
    return 30+(p.scale-1)*4+(p.spacing-1)*6;
}

void UI::restorePreferences(Preferences preferences) {
    if (!validFont(preferences.font)) preferences.font=ReadingFont::Serif;
    if (preferences.scale<0 || preferences.scale>2) preferences.scale=1;
    if (preferences.spacing<0 || preferences.spacing>2) preferences.spacing=1;
    preferences_=preferences;
}

void UI::changePreferences(Preferences preferences) {
    if (preferences.font==preferences_.font && preferences.dark==preferences_.dark &&
        preferences.scale==preferences_.scale && preferences.spacing==preferences_.spacing) return;
    restorePreferences(preferences);
    services_.savePreferences(preferences_);
}

void UI::restoreSession(Screen resume) {
    resume_=resume==Screen::Reader || resume==Screen::Scribe ? resume : Screen::Library;
}

void UI::finishBoot() {
    if (screen_==Screen::Boot) show(resume_);
}

void UI::endStroke() {
    if (!drawing_) return;
    drawing_=false;
    editInk(InkAction::End);
}

void UI::editInk(InkAction action) {
    services_.ink({action,0,0,size_,erase_});
    services_.ink({InkAction::Checkpoint,0,0,size_,erase_});
}

void UI::show(Screen screen) {
    if (screen==Screen::Settings && screen_!=Screen::Settings) {
        returnFromSettings_=screen_==Screen::Boot ? Screen::Library : screen_;
    }
    endStroke();
    touching_=false;
    screen_=screen;
    confirmClear_=false;
    message_=nullptr;
    if (screen==Screen::Reader || screen==Screen::Scribe) lastMode_=screen;
    if (services_.screenChanged) services_.screenChanged(screen);
}

void UI::saveResult(bool success) {
    message_ = success ? "NOTE SAVED" : "SAVE FAILED / CANVAS KEPT";
}

void UI::button(Button button, bool held) {
    if (screen_==Screen::Boot) { finishBoot(); return; }
    if (screen_==Screen::Settings) {
        if (button==Button::Mode) show(returnFromSettings_);
        else {
            const int delta=button==Button::Prev ? -1 : 1;
            changePreferences({ReadingFont((int(preferences_.font)+delta+fontCount)%fontCount),preferences_.dark,preferences_.scale,preferences_.spacing});
        }
        return;
    }
    if (screen_==Screen::Templates || screen_==Screen::Rename) {
        if (button==Button::Mode) show(Screen::Scribe);
        return;
    }
    if (screen_==Screen::Scribe && button==Button::Mode && services_.notebook().annotation) {
        closeScribe();
        return;
    }
    if (screen_==Screen::Library && button==Button::Mode && held) {
        show(Screen::Settings);
        return;
    }
    if (confirmClear_) {
        if (button==Button::Next) editInk(InkAction::Clear);
        confirmClear_=false;
        return;
    }
    if (button==Button::Mode) {
        if (held) show(Screen::Library);
        else if (screen_==Screen::Library) show(lastMode_);
        else show(screen_==Screen::Reader ? Screen::Scribe : Screen::Reader);
        return;
    }
    if (screen_==Screen::Library) {
        show(button==Button::Prev ? Screen::Reader : Screen::Scribe);
    } else if (screen_==Screen::Reader) {
        services_.turn(button==Button::Prev ? -1 : 1);
    } else if (button==Button::Next) {
        services_.ink({InkAction::Save,0,0,size_,false});
    } else if (held) confirmClear_=true;
    else editInk(InkAction::Undo);
}

void UI::tap(int x, int y) {
    if (screen_==Screen::Boot) { finishBoot(); return; }
    if (screen_==Screen::Settings) {
        if (within(x,y,24,8,140,45) || within(x,y,400,8,56,45) || within(x,y,28,724,168,53)) {
            show(returnFromSettings_);
        } else if (within(x,y,28,fontTop,424,fontCount*fontRow)) {
            changePreferences({ReadingFont((y-fontTop)/fontRow),preferences_.dark,preferences_.scale,preferences_.spacing});
        } else if (within(x,y,28,themeTop,424,48)) {
            changePreferences({preferences_.font,x>=240,preferences_.scale,preferences_.spacing});
        } else if (within(x,y,28,431,424,45)) {
            changePreferences({preferences_.font,preferences_.dark,(x-28)/142,preferences_.spacing});
        } else if (within(x,y,28,532,424,45)) {
            changePreferences({preferences_.font,preferences_.dark,preferences_.scale,(x-28)/142});
        }
        return;
    }
    if (screen_==Screen::Templates) {
        if (y>=182 && y<442 && x>=28 && x<452) {
            services_.notebookAction(NotebookAction::SetTemplate,(y-182)/65);
            show(Screen::Scribe);
        } else if (x>=400 && y<54) show(Screen::Scribe);
        return;
    }
    if (screen_==Screen::Rename) {
        if (x>=400 && y<54) show(Screen::Scribe);
        else if (within(x,y,28,526,195,54)) nameKey(' ');
        else if (within(x,y,230,526,92,54)) nameKey('\b');
        else if (within(x,y,329,526,123,54) && nameLength_>0) {
            services_.renamePage(name_);
            show(Screen::Scribe);
        } else {
            for (int row=0;row<3;++row) {
                const int left=29+row*16, count=10-row-(row==2 ? 1 : 0);
                if (within(x,y,left,340+row*62,count*42,53))
                    nameKey(keys[row][(x-left)/42]);
            }
        }
        return;
    }
    if (confirmClear_) {
        if (within(x,y,77,440,151,44)) confirmClear_=false;
        if (within(x,y,251,440,151,44)) button(Button::Next,false);
        return;
    }
    if (within(x,y,211,8,83,43)) { show(Screen::Settings); return; }
    if (screen_==Screen::Library) {
        if (within(x,y,400,8,56,45)) show(Screen::Settings);
        else if (within(x,y,26,186,62,45)) setFilter(Filter::All);
        else if (within(x,y,96,186,83,45)) setFilter(Filter::Books);
        else if (within(x,y,187,186,109,45)) setFilter(Filter::Notebooks);
        else if (within(x,y,28,libraryTop,424,libraryBottom-libraryTop)) {
            const int documentY=y+libraryOffset_;
            if (documentY>=listTop) selectEntry((documentY-listTop)/rowHeight);
            else if (within(x,documentY,28,254,204,310))
                selectEntry(0);
            else if (filter_==Filter::All && within(x,documentY,248,254,204,310))
                selectEntry(1);
        }
        return;
    }
    if (within(x,y,371,8,45,45)) { show(Screen::Settings); return; }
    if (within(x,y,24,8,140,45) || within(x,y,416,8,40,45)) {
        if (screen_==Screen::Scribe) closeScribe();
        else show(Screen::Library);
        return;
    }
    if (screen_==Screen::Reader) {
        if (within(x,y,22,720,73,64)) button(Button::Prev,false);
        else if (within(x,y,385,720,73,64)) button(Button::Next,false);
        else if (within(x,y,300,104,152,37)) services_.bookmark(BookmarkAction::Toggle);
        else if (within(x,y,100,764,280,32)) services_.bookmark(BookmarkAction::Next);
        else if (within(x,y,28,232,424,455)) {
            const Page page=services_.page(preferences_);
            const int line=(y-232)/lineStep(preferences_);
            if (line<page.lineCount && page.paragraphAnchors && page.paragraphAnchors[line]>=0) {
                if (services_.annotate(page.paragraphAnchors[line])) show(Screen::Scribe);
            }
        }
    } else if (screen_==Screen::Scribe && y>=111 && y<152 && !services_.notebook().annotation) {
        if (x>=260 && x<306) services_.notebookAction(NotebookAction::Previous,0);
        else if (x>=306 && x<352) services_.notebookAction(NotebookAction::Next,0);
        else if (x>=352 && x<400) services_.notebookAction(NotebookAction::NewPage,0);
        else if (x>=400 && x<456) show(Screen::Templates);
        else if (x>=28 && x<250) beginRename();
    } else if (y>=688 && y<758) {
        if (x>=26 && x<86) erase_=false;
        else if (x<146 && x>=86) erase_=true;
        else if (x<206 && x>=146) size_=size_==3 ? 6 : size_==6 ? 10 : 3;
        else if (x<266 && x>=206) button(Button::Prev,false);
        else if (x>=274 && x<336) button(Button::Prev,true);
        else if (x>=347 && x<454) button(Button::Next,false);
    }
}

void UI::setFilter(Filter filter) {
    if (filter==filter_) return;
    filter_=filter;
    libraryOffset_=0;
}

void UI::selectEntry(int index) {
    if (index<0 || index>=services_.entryCount(filter_)) return;
    const LibraryEntry entry=services_.entry(filter_,index);
    if (services_.openEntry(entry)) show(entry.destination);
}

void UI::scroll(int delta) {
    if (screen_!=Screen::Library) return;
    int limit=contentBottom(services_.entryCount(filter_))-libraryBottom;
    if (limit<0) limit=0;
    const int next=libraryOffset_+delta;
    libraryOffset_=next<0 ? 0 : next>limit ? limit : next;
}

void UI::pointer(int phase, int x, int y) {
    if (phase==0) {
        if (touching_) return;
        touching_=true;
        dragged_=false;
        touchX_=x;
        touchY_=y;
        if (screen_==Screen::Scribe && !confirmClear_ && within(x,y,28,153,424,505)) {
            message_=nullptr;
            drawing_=true;
            services_.ink({InkAction::Begin,x,y,size_,erase_});
        }
        return;
    }
    if (!touching_) return;

    const int dx=x-touchX_, dy=y-touchY_;
    dragged_=dragged_ || magnitude(dx)>13 || magnitude(dy)>13;
    if (drawing_) {
        if (phase==1 && within(x,y,28,153,424,505)) {
            services_.ink({InkAction::Move,x,y,size_,erase_});
        } else {
            endStroke();
            dragged_=true;
        }
    }
    if (phase==1) return;
    touching_=false;
    if (phase!=2) return; // Cancellation never selects or navigates.

    if (screen_==Screen::Library && touchY_>=libraryTop && touchY_<libraryBottom &&
        magnitude(dy)>=26 && magnitude(dy)>magnitude(dx)) {
        scroll(-dy);
    } else if (screen_==Screen::Reader && touchY_>=204 && touchY_<704 &&
               magnitude(dx)>=48 && magnitude(dx)>magnitude(dy)*2) {
        services_.turn(dx<0 ? 1 : -1);
    } else if (!dragged_) {
        tap(x,y);
    }
}

void UI::draw(const Display& d) const {
    d.palette(preferences_.dark);
    d.fill(0,0,width,height,false);
    const int reported = screen_!=Screen::Boot && services_.batteryPercent ? services_.batteryPercent() : -1;
    const int battery = reported>=0 && reported<=100 ? reported : -1;
    switch (screen_) {
    case Screen::Boot:
        d.icon(Icon::Logo,190,289,100,true);
        d.text(128,445,49,Font::Serif,"DSH-Prose",true);
        d.text(162,482,13,Font::Label,"DIGITAL NOTEBOOK",true);
        break;
    case Screen::Library: drawLibrary(d,battery); break;
    case Screen::Reader: drawReader(d,battery); break;
    case Screen::Scribe: drawScribe(d,battery); break;
    case Screen::Settings: drawSettings(d,battery); break;
    case Screen::Templates: drawTemplates(d,battery); break;
    case Screen::Rename: drawRename(d,battery); break;
    }
}

void UI::drawLibrary(const Display& d, int battery) const {
    const auto items=services_.library();
    const int top=254-libraryOffset_;
    if (top+305>libraryTop) {
        if (filter_!=Filter::Notebooks) {
            bookPreview(d,28,top,items);
            d.text(28,top+269,24,Font::Serif,items.bookTitle,true);
            d.text(28,top+297,12,Font::Label,"BOOK  /  TXT",true);
        }
        if (filter_!=Filter::Books) {
            const int x=filter_==Filter::Notebooks ? 28 : 248;
            notebookPreview(d,x,top);
            d.text(x,top+269,24,Font::Serif,items.notebookTitle,true);
            d.text(x,top+297,12,Font::Label,"NOTEBOOK",true);
        }
    }
    const int count=services_.entryCount(filter_);
    if (count>0) d.text(28,listTop-21-libraryOffset_,12,Font::Label,"BROWSE",true);
    const int hiddenRows=(libraryOffset_+libraryTop-listTop)/rowHeight;
    for (int i=hiddenRows>0 ? hiddenRows : 0;i<count;++i) {
        const int y=listTop+i*rowHeight-libraryOffset_;
        if (y>=libraryBottom) break;
        const auto entry=services_.entry(filter_,i);
        d.line(28,y,452,y,1);
        const auto icon=entry.destination==Screen::Reader ? Icon::Book : Icon::Notebook;
        d.icon(icon,33,y+17,22,true);
        d.text(68,y+26,20,Font::Serif,entry.title,true);
        d.text(68,y+44,10,Font::Label,entry.destination==Screen::Reader ? "BOOK" : "NOTEBOOK",true);
        d.icon(Icon::Right,422,y+18,20,true);
    }
    // Pin the header and footer over the moving content.
    d.fill(0,0,width,libraryTop,false);
    d.fill(0,libraryBottom,width,height-libraryBottom,false);
    header(d,"LIBRARY",Icon::Settings,battery);
    d.text(28,125,61,Font::Serif,"Library",true);
    d.text(31,159,16,Font::Sans,"Your books and notebooks.",true);
    chip(d,28,192,58,"All",filter_==Filter::All);
    chip(d,98,192,80,"Books",filter_==Filter::Books);
    chip(d,190,192,107,"Notebooks",filter_==Filter::Notebooks);
    const int maxOffset=contentBottom(count)-libraryBottom;
    if (maxOffset>0) {
        const int bar=72;
        d.fill(468,libraryTop+(libraryBottom-libraryTop-bar)*libraryOffset_/maxOffset,3,bar,true);
    }
    d.line(28,libraryBottom,452,libraryBottom,1);
    d.text(28,779,12,Font::Label,"DSH-PROSE",true);
}

void UI::drawReader(const Display& d, int battery) const {
    header(d,"READER",Icon::Home,battery);
    const auto font=readingFonts[int(preferences_.font)];
    const Page page=services_.page(preferences_);
    d.text(32,111,13,Font::Label,page.chapter,true);
    d.text(31,173,47,Font::Serif,page.title,true);
    d.line(32,203,448,203,1);
    chip(d,305,106,147,page.bookmarked ? "Bookmarked" : "Bookmark",page.bookmarked);
    const int step=lineStep(preferences_);
    for (int i=0;i<page.lineCount && 253+i*step<689;++i) {
        const int y=253+i*step;
        d.text(34,y,readingSize(preferences_),font.face,page.lines[i],true);
        if (page.annotated && page.annotated[i]) d.fill(447,y-9,4,7,true);
    }
    d.line(28,704,452,704,1);
    const int progress=page.count>0 ? 420*page.number/page.count : 0;
    d.fill(29,703,progress,3,true);
    d.icon(Icon::Left,31,737,23,true);
    d.icon(Icon::Right,425,737,23,true);
    char label[38];
    char* end=label;
    for (char c : "PAGE ") if (c) *end++=c;
    decimal(end,page.number);
    *end++=' '; *end++='/'; *end++=' ';
    decimal(end,page.count);
    *end='\0';
    d.text(199,753,13,Font::Label,label,true);
    d.text(168,784,12,Font::Label,page.bookmarkCount ? "NEXT BOOKMARK" : "TAP TEXT TO ANNOTATE",true);
}

void UI::drawScribe(const Display& d, int battery) const {
    header(d,"SCRIBE",Icon::Home,battery);
    const auto page=services_.notebook();
    d.text(29,103,37,Font::Serif,page.annotation ? "Margin note" : services_.library().activeNotebookTitle,true);
    if (page.annotation) d.text(31,135,13,Font::Label,"ATTACHED TO THIS PARAGRAPH",true);
    else {
        d.text(29,134,15,Font::Sans,page.name,true);
        d.icon(Icon::Pen,225,120,16,true);
        d.icon(Icon::Left,269,117,24,true);
        d.icon(Icon::Right,315,117,24,true);
        chip(d,352,114,42,"+",false);
        chip(d,400,114,52,"Paper",false);
    }
    outline(d,28,153,424,505);
    if (page.paper==Paper::DotGrid) {
        for (int x=40;x<451;x+=16)
            for (int y=165;y<657;y+=16) d.fill(x,y,1,1,true);
    } else if (page.paper==Paper::Lined || page.paper==Paper::Checklist) {
        for (int y=191;y<651;y+=34) {
            const bool checklist=page.paper==Paper::Checklist;
            if (checklist) outline(d,43,y-14,13,13);
            d.line(checklist ? 67 : 43,y,438,y,1);
        }
    }
    d.ink();
    d.line(28,684,452,684,1);
    if (!erase_) d.fill(29,698,50,48,true);
    else d.fill(89,698,50,48,true);
    d.icon(Icon::Pen,42,710,24,erase_);
    d.icon(Icon::Eraser,102,710,24,!erase_);
    d.line(159,724,185,724,size_);
    d.icon(Icon::Undo,220,710,24,true);
    d.icon(Icon::Close,291,710,24,true);
    d.fill(351,700,101,44,true);
    d.icon(Icon::Save,364,711,21,false);
    d.text(393,729,15,Font::Sans,"Save",false);
    if (message_) d.text(28,773,12,Font::Label,message_,true);
    if (!page.annotation) {
        char label[28] = "PAGE ";
        char* end=label+5;
        decimal(end,page.number);
        *end++='/';
        decimal(end,page.count);
        *end='\0';
        d.text(360,773,12,Font::Label,label,true);
    }
    if (confirmClear_) {
        d.fill(61,320,358,181,false);
        outline(d,61,320,358,181);
        d.text(78,369,31,Font::Serif,"Clear this page?",true);
        d.text(78,409,17,Font::Sans,"This clears the current canvas.",true);
        chip(d,77,445,149,"Cancel",false);
        chip(d,251,445,151,"Clear page",true);
    }
}
void UI::drawSettings(const Display& d, int battery) const {
    header(d,"SETTINGS",Icon::Close,battery);
    d.text(28,125,57,Font::Serif,"Settings",true);
    d.text(30,174,13,Font::Label,"READING FONT",true);
    for (int i=0;i<fontCount;++i) {
        const int y=fontTop+i*fontRow;
        const bool selected=i==int(preferences_.font);
        if (selected) d.fill(28,y+1,424,fontRow-2,true);
        d.line(28,y,452,y,1);
        d.text(43,y+35,20,readingFonts[i].face,readingFonts[i].label,!selected);
        if (selected) d.text(362,y+34,11,Font::Label,"SELECTED",false);
    }
    d.line(28,fontTop+fontCount*fontRow,452,fontTop+fontCount*fontRow,1);
    d.text(30,423,13,Font::Label,"TEXT SIZE",true);
    chip(d,28,438,132,"Small",preferences_.scale==0);
    chip(d,174,438,132,"Medium",preferences_.scale==1);
    chip(d,320,438,132,"Large",preferences_.scale==2);
    d.text(30,525,13,Font::Label,"LINE SPACING",true);
    chip(d,28,539,132,"Compact",preferences_.spacing==0);
    chip(d,174,539,132,"Normal",preferences_.spacing==1);
    chip(d,320,539,132,"Roomy",preferences_.spacing==2);
    d.text(30,615,13,Font::Label,"APPEARANCE",true);
    chip(d,28,themeTop+5,202,"Light",!preferences_.dark);
    chip(d,250,themeTop+5,202,"Dark",preferences_.dark);
    const auto font=readingFonts[int(preferences_.font)];
    d.text(30,707,readingSize(preferences_),font.face,"A quiet place to read.",true);
    d.line(28,727,452,727,1);
    d.icon(Icon::Left,28,743,22,true);
    d.text(63,761,16,Font::Sans,"Done",true);
    d.text(267,759,12,Font::Label,"BATTERY",true);
    char label[6];
    batteryLabel(label,battery);
    d.text(327,759,12,Font::Label,battery<0 ? "UNAVAILABLE" : label,true);
}
void UI::closeScribe() {
    if (services_.notebook().annotation) {
        if (services_.annotate(-1)) show(Screen::Reader);
    } else show(Screen::Library);
}

void UI::beginRename() {
    const char* name=services_.notebook().name;
    nameLength_=0;
    while (nameLength_<24 && name[nameLength_]) {
        const unsigned char c=name[nameLength_];
        if (c>127) break;
        name_[nameLength_++]=char(c);
    }
    name_[nameLength_]='\0';
    show(Screen::Rename);
}

void UI::nameKey(char key) {
    if (key=='\b') {
        if (nameLength_>0) --nameLength_;
    } else if (nameLength_<24) name_[nameLength_++]=key;
    name_[nameLength_]='\0';
}

void UI::drawTemplates(const Display& d,int battery) const {
    header(d,"PAPER",Icon::Close,battery);
    d.text(29,124,48,Font::Serif,"Page template",true);
    for (int i=0;i<4;++i) {
        const int y=182+i*65;
        const bool active=i==int(services_.notebook().paper);
        d.line(28,y,452,y,1);
        d.text(40,y+41,24,Font::Serif,paperNames[i],true);
        if (active) d.text(350,y+40,12,Font::Label,"SELECTED",true);
    }
}

void UI::drawRename(const Display& d,int battery) const {
    header(d,"RENAME",Icon::Close,battery);
    d.text(29,124,48,Font::Serif,"Rename page",true);
    outline(d,28,184,424,64);
    d.text(43,225,22,Font::Sans,name_,true);
    for (int row=0;row<3;++row) {
        const int left=29+row*16, count=10-row-(row==2 ? 1 : 0);
        for (int col=0;col<count;++col) {
            const char label[]={keys[row][col],'\0'};
            chip(d,left+col*42,340+row*62,38,label,false);
        }
    }
    chip(d,28,536,195,"Space",false);
    chip(d,230,536,92,"Delete",false);
    chip(d,329,536,123,"Done",true);
}
} // namespace prose
