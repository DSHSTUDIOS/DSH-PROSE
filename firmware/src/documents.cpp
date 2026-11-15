#include "documents.hpp"
#include "board.hpp"
#include "journal.hpp"
#include "panel.hpp"

#include <Arduino.h>
#include <Preferences.h>
#include <SD_MMC.h>
#include <algorithm>
#include <cstring>
#include <vector>

namespace documents {
namespace {
using namespace prose;
constexpr int notebookCount=3;
constexpr unsigned maxBooks=32, maxPages=64, maxMarks=24;
constexpr size_t maxBookBytes=256*1024;
constexpr size_t inkBytes=(board::canvasWidth+7)/8*board::canvasHeight;
const char* notes[]={"Field Notes","Sketchbook","Ideas"};
const char guide[]="Read and write on DSH-Prose.\n\nPut plain text books in /books on your microSD card. "
    "Insert the card before turning the device on.\n\nSwipe to turn pages. Tap a paragraph to write a margin note.\n\n"
    "In Scribe, tap + to add a page or Paper to choose a template.";

struct Book {
    String path, title;
};
struct Line {
    uint32_t start, end;
    int anchor;
};
struct Positions {
    uint32_t offset;
    uint32_t marks[maxMarks];
    uint8_t count;
};
struct SavedSession {
    char path[128];
    int32_t annotation;
    uint8_t screen, notebook;
};
struct Index {
    uint8_t active, count;
};
struct InkRecord {
    char name[25];
    uint8_t paper;
    uint8_t bits[inkBytes];
};
static_assert(sizeof(InkRecord)==26+inkBytes,"Unexpected page layout");

std::vector<Book> books;
std::vector<Line> lines;
std::vector<size_t> pageStarts;
::Preferences kv;
bool cardMounted=false, nvs=false, layoutReady=false, dirty=false;
prose::UI* ui=nullptr;
int bookIndex=0, noteIndex=0, annotation=-1;
prose::Screen sessionScreen=Screen::Library;
prose::Preferences lastLayout;
Index indexes[notebookCount]{};
char* bookText=nullptr;
const char* source=guide;
size_t sourceLength=sizeof(guide)-1;
uint32_t bookId=0, revision=0;
Positions positions{};
char bookKey[12], pagePath[74];
String bookTitle="Quick start", cover1="Quick start", cover2="";
InkRecord* record=nullptr;
GFXcanvas1 canvas(board::canvasWidth,board::canvasHeight), undoCanvas(board::canvasWidth,board::canvasHeight);
bool canUndo=false;
int lastX=0,lastY=0;
char rowText[24][126];
const char* rowPointers[24];
int anchors[24];
bool noted[24];
size_t displayedPage=0;

bool cardAvailable() { return cardMounted && digitalRead(board::sdDetect)==LOW; }
void saveSession() {
    if (!nvs) return;
    SavedSession session{};
    books[bookIndex].path.toCharArray(session.path,sizeof(session.path));
    session.annotation=annotation;
    session.screen=uint8_t(sessionScreen);
    session.notebook=noteIndex;
    kv.putBytes("session", &session,sizeof(session));
}
void savePositions() {
    if (nvs) kv.putBytes(bookKey,&positions,sizeof(positions));
}
void discover() {
    if (cardAvailable()) {
        auto root=SD_MMC.open("/books");
        if (root && root.isDirectory()) {
            for (auto f=root.openNextFile(); f && books.size()<maxBooks; f=root.openNextFile()) {
                String path=f.path(), title=f.name();
                if (f.isDirectory() || !f.size() || f.size()>maxBookBytes || path.length()>=128) continue;
                String lower=title;
                lower.toLowerCase();
                if (!lower.endsWith(".txt")) continue;
                title.remove(title.length()-4);
                books.push_back({path,title});
            }
        }
    }
    std::sort(books.begin(),books.end(),[](const Book& a,const Book& b){return a.title<b.title;});
    if (books.empty()) books.push_back({"","Quick start"});
    const String& title=books[0].title;
    cover1=title.substring(0,14);
    const int split=cover1.lastIndexOf(' ');
    if (title.length()>14 && split>0) cover1=title.substring(0,split);
    cover2=title.substring(cover1.length());
    cover2.trim();
    if (cover2.length()>12) cover2=cover2.substring(0,9)+"...";
}
void openBook(int index) {
    bookIndex=index;
    const auto& entry=books[index];
    bookTitle=entry.title;
    if (bookTitle.length()>25) bookTitle=bookTitle.substring(0,22)+"...";
    source=guide;
    sourceLength=sizeof(guide)-1;
    if (entry.path.length() && cardAvailable()) {
        auto file=SD_MMC.open(entry.path);
        if (file && file.size()>0 && file.size()<=maxBookBytes) {
            const auto length=file.size();
            if (file.read(reinterpret_cast<uint8_t*>(bookText),length)==length) {
                source=bookText;
                sourceLength=length;
            }
        }
    }
    bookId=journal::checksum(source,sourceLength)^journal::checksum(entry.path.c_str(),entry.path.length());
    snprintf(bookKey,sizeof(bookKey),"b%08lx",static_cast<unsigned long>(bookId));
    positions={};
    if (nvs && kv.getBytesLength(bookKey)==sizeof(positions)) kv.getBytes(bookKey,&positions,sizeof(positions));
    if (positions.count>maxMarks || positions.offset>=sourceLength) positions={};
    layoutReady=false;
}

bool saveInk() {
    if (!dirty) return true;
    if (!cardAvailable()) return false;
    memcpy(record->bits,canvas.getBuffer(),inkBytes);
    if (!journal::write(SD_MMC,pagePath,record,sizeof(*record),revision)) return false;
    dirty=false;
    return true;
}
void loadInk() {
    const auto selected=indexes[noteIndex];
    if (annotation>=0) {
        snprintf(pagePath,sizeof(pagePath),"/notes/a%08lx-%08lx",
                 static_cast<unsigned long>(bookId),static_cast<unsigned long>(annotation));
    } else {
        snprintf(pagePath,sizeof(pagePath),"/notes/n%d-p%02u",noteIndex,selected.active+1);
    }
    revision=0;
    memset(record,0,sizeof(*record));
    const bool found=cardAvailable() && journal::read(SD_MMC,pagePath,record,sizeof(*record),revision);
    if (!found || record->paper>3) {
        memset(record,0,sizeof(*record));
        record->paper=annotation<0 ? int(Paper::DotGrid) : int(Paper::Lined);
        if (annotation<0) snprintf(record->name,sizeof(record->name),"Page %d",selected.active+1);
        else strcpy(record->name,"Margin note");
    }
    record->name[24]=0;
    memcpy(canvas.getBuffer(),record->bits,inkBytes);
    canUndo=false;
    dirty=false;
}
void saveIndexes() {
    if (nvs) kv.putBytes("notebooks",indexes,sizeof(indexes));
}
void failed() { ui->saveResult(false); }
void exportHeader(uint8_t* bytes,unsigned field,uint32_t value,unsigned count) {
    for (unsigned i=0;i<count;++i) bytes[field+i]=uint8_t(value>>(8*i));
}
bool exportBmp() {
    if (!cardAvailable()) return false;
    char path[85], stage[85], backup[85];
    snprintf(path,sizeof(path),"%s.bmp",pagePath);
    snprintf(stage,sizeof(stage),"%s.tmp",pagePath);
    snprintf(backup,sizeof(backup),"%s.bak",pagePath);
    auto file=SD_MMC.open(stage,"w");
    if (!file) return false;
    constexpr int rowBytes=((board::canvasWidth+31)/32)*4;
    constexpr int sourceBytes=(board::canvasWidth+7)/8;
    uint8_t header[62]={};
    header[0]='B'; header[1]='M';
    exportHeader(header,2,sizeof(header)+rowBytes*board::canvasHeight,4);
    exportHeader(header,10,sizeof(header),4);
    exportHeader(header,14,40,4);
    exportHeader(header,18,board::canvasWidth,4);
    exportHeader(header,22,board::canvasHeight,4);
    exportHeader(header,26,1,2);
    exportHeader(header,28,1,2);
    exportHeader(header,34,rowBytes*board::canvasHeight,4);
    header[54]=header[55]=header[56]=255;
    bool ok=file.write(header,sizeof(header))==sizeof(header);
    uint8_t row[rowBytes]={};
    for (int y=board::canvasHeight-1; ok && y>=0; --y) {
        memcpy(row,canvas.getBuffer()+y*sourceBytes,sourceBytes);
        ok=file.write(row,sizeof(row))==sizeof(row);
    }
    file.flush();
    file.close();
    if (!ok) return false;
    if (SD_MMC.exists(path)) {
        SD_MMC.remove(backup);
        if (!SD_MMC.rename(path,backup)) return false;
    }
    if (!SD_MMC.rename(stage,path)) {
        if (SD_MMC.exists(backup) && !SD_MMC.exists(path)) SD_MMC.rename(backup,path);
        return false;
    }
    SD_MMC.remove(backup);
    return true;
}
void drawStroke(int x,int y,int diameter,bool erase) {
    const int distance=max(abs(x-lastX),abs(y-lastY));
    const int steps=max(1,distance);
    for (int i=1;i<=steps;++i) {
        const int px=lastX+(x-lastX)*i/steps, py=lastY+(y-lastY)*i/steps;
        canvas.fillCircle(px,py,diameter/2,erase ? 0 : 1);
    }
    lastX=x; lastY=y;
}
void ink(InkEvent event) {
    const int x=event.x-board::canvasX, y=event.y-board::canvasY;
    const int diameter=event.erase ? max(12,event.size*3) : event.size;
    switch (event.action) {
    case InkAction::Begin:
    case InkAction::Clear:
        memcpy(undoCanvas.getBuffer(),canvas.getBuffer(),inkBytes);
        canUndo=true;
        dirty=true;
        if (event.action==InkAction::Clear) canvas.fillScreen(0);
        else { lastX=x; lastY=y; drawStroke(x,y,diameter,event.erase); }
        break;
    case InkAction::Move: drawStroke(x,y,diameter,event.erase); dirty=true; break;
    case InkAction::End: break;
    case InkAction::Undo:
        if (canUndo) {
            memcpy(canvas.getBuffer(),undoCanvas.getBuffer(),inkBytes);
            canUndo=false;
            dirty=true;
        }
        break;
    case InkAction::Save:
        if (saveInk() && exportBmp()) ui->saveResult(true);
        else failed();
        break;
    case InkAction::Checkpoint:
        if (!saveInk()) failed();
        break;
    }
}

void lineText(char* output,size_t cap,size_t first,size_t last) {
    size_t count=0;
    for (size_t i=first;i<last && count+1<cap;++i) {
        const unsigned char c=source[i];
        if (c>=32 && c<=126) output[count++]=char(c);
        else if (c=='\t') output[count++]=' ';
        else if (c>=0xc0) output[count++]='?';
    }
    output[count]=0;
}
void paginate(prose::Preferences prefs) {
    if (layoutReady && prefs.font==lastLayout.font && prefs.scale==lastLayout.scale && prefs.spacing==lastLayout.spacing) return;
    lastLayout=prefs;
    lines.clear(); pageStarts.clear();
    const int perPage=1+(688-253)/lineStep(prefs);
    const int fontSize=readingSize(prefs);
    const Font face=readingFace(prefs.font);
    for (size_t start=0; start<sourceLength;) {
        size_t paragraphEnd=start;
        while (paragraphEnd<sourceLength && source[paragraphEnd]!='\n') ++paragraphEnd;
        if (paragraphEnd==start) lines.push_back({uint32_t(start),uint32_t(start),-1});
        for (size_t from=start;from<paragraphEnd;) {
            while (from<paragraphEnd && (source[from]==' ' || source[from]=='\t')) ++from;
            if (from==paragraphEnd) break;
            size_t best=from, space=from;
            while (best<paragraphEnd && best-from<110) {
                char buf[126];
                lineText(buf,sizeof(buf),from,best+1);
                if (best>from && panel::measure(buf,fontSize,face)>410) break;
                if (source[best]==' ' || source[best]=='\t') space=best;
                ++best;
            }
            if (best<paragraphEnd && space>from) best=space;
            else while (best<paragraphEnd && (static_cast<uint8_t>(source[best])&0xc0)==0x80) ++best;
            lines.push_back({uint32_t(from),uint32_t(best),int(start)});
            from=best;
        }
        start=paragraphEnd+1;
    }
    if (lines.empty()) lines.push_back({0,0,-1});
    for (size_t i=0; i<lines.size();) {
        while (i+1<lines.size() && lines[i].anchor<0) ++i;
        pageStarts.push_back(i);
        i+=perPage;
    }
    layoutReady=true;
}
bool hasAnnotation(int anchor) {
    if (anchor<0 || !cardAvailable()) return false;
    char name[88];
    snprintf(name,sizeof(name),"/notes/a%08lx-%08lx",static_cast<unsigned long>(bookId),static_cast<unsigned long>(anchor));
    char slot[90];
    snprintf(slot,sizeof(slot),"%s.0",name);
    if (SD_MMC.exists(slot)) return true;
    snprintf(slot,sizeof(slot),"%s.1",name);
    return SD_MMC.exists(slot);
}
Page page(prose::Preferences prefs) {
    paginate(prefs);
    displayedPage=0;
    while (displayedPage+1<pageStarts.size() && lines[pageStarts[displayedPage+1]].start<=positions.offset) ++displayedPage;
    const size_t first=pageStarts[displayedPage];
    const size_t end=displayedPage+1<pageStarts.size() ? pageStarts[displayedPage+1] : lines.size();
    int count=0;
    int previousAnchor=-2;
    bool hasNote=false;
    for (size_t i=first; i<end && count<24; ++i,++count) {
        const auto line=lines[i];
        lineText(rowText[count],sizeof(rowText[count]),line.start,line.end);
        rowPointers[count]=rowText[count];
        anchors[count]=line.anchor;
        if (line.anchor!=previousAnchor) { hasNote=hasAnnotation(line.anchor); previousAnchor=line.anchor; }
        noted[count]=hasNote;
    }
    const auto start=lines[first].start;
    const auto stop=end<lines.size() ? lines[end].start : sourceLength;
    bool marked=false;
    for (int i=0;i<positions.count;++i) if (positions.marks[i]>=start && positions.marks[i]<stop) marked=true;
    return {bookTitle.c_str(),books[bookIndex].path.length() ? "BOOK" : "GETTING STARTED",
            rowPointers,count,int(displayedPage+1),int(pageStarts.size()),anchors,noted,marked,positions.count};
}
void turn(int delta) {
    const int next=int(displayedPage)+delta;
    if (!layoutReady || next<0 || next>=int(pageStarts.size())) return;
    positions.offset=lines[pageStarts[next]].start;
    savePositions();
}
void bookmark(BookmarkAction action) {
    if (action==BookmarkAction::Next) {
        if (positions.count==0) return;
        uint32_t next=positions.marks[0];
        for (int i=0;i<positions.count;++i)
            if (positions.marks[i]>positions.offset) { next=positions.marks[i]; break; }
        positions.offset=next;
    } else {
        if (!layoutReady) return;
        const auto start=lines[pageStarts[displayedPage]].start;
        const auto end=displayedPage+1<pageStarts.size() ? lines[pageStarts[displayedPage+1]].start : sourceLength;
        int remove=-1;
        for (int i=0;i<positions.count;++i) if (positions.marks[i]>=start && positions.marks[i]<end) { remove=i; break; }
        if (remove>=0) {
            for (int i=remove;i+1<positions.count;++i) positions.marks[i]=positions.marks[i+1];
            --positions.count;
        } else if (positions.count<maxMarks) {
            positions.marks[positions.count++]=positions.offset;
            std::sort(positions.marks,positions.marks+positions.count);
        }
    }
    savePositions();
}

LibraryItems library() { return {books[0].title.c_str(),cover1.c_str(),cover2.c_str(),notes[0],notes[noteIndex]}; }
int entryCount(Filter filter) {
    if (filter==Filter::Books) return books.size();
    if (filter==Filter::Notebooks) return notebookCount;
    return books.size()+notebookCount;
}
LibraryEntry entry(Filter filter,int index) {
    if (filter==Filter::Notebooks || (filter==Filter::All && index==1))
        return {notes[filter==Filter::All ? 0 : index],Screen::Scribe,filter==Filter::All ? 0 : index};
    const int book=filter==Filter::All && index>1 ? index-1 : index;
    if (book>=int(books.size())) {
        const int note=index-books.size();
        return {notes[note],Screen::Scribe,note};
    }
    return {books[book].title.c_str(),Screen::Reader,book};
}
bool openEntry(LibraryEntry item) {
    if (!saveInk()) { failed(); return false; }
    const bool wasAnnotation=annotation>=0;
    annotation=-1;
    if (item.destination==Screen::Reader) {
        if (item.id!=bookIndex) openBook(item.id);
        if (wasAnnotation) loadInk();
    } else if (item.id!=noteIndex || wasAnnotation) {
        noteIndex=item.id;
        loadInk();
    }
    return true;
}
bool annotate(int anchor) {
    if (anchor==annotation) return true;
    if (!saveInk()) { failed(); return false; }
    annotation=anchor;
    loadInk();
    return true;
}
NotebookPage notebook() {
    const auto index=indexes[noteIndex];
    return {record->name,index.active+1,index.count,Paper(record->paper),annotation>=0};
}
void renamePage(const char* name) {
    if (annotation>=0) return;
    snprintf(record->name,sizeof(record->name),"%s",name);
    dirty=true;
    if (!saveInk()) failed();
}
void notebookAction(NotebookAction action,int value) {
    if (annotation>=0 || !saveInk()) { failed(); return; }
    if (action==NotebookAction::SetTemplate) {
        if (value<0 || value>3) return;
        record->paper=value;
        dirty=true;
        if (!saveInk()) failed();
        return;
    }
    auto& index=indexes[noteIndex];
    if (action==NotebookAction::NewPage) {
        if (!cardAvailable() || index.count>=maxPages) { failed(); return; }
        const auto before=index;
        const uint8_t paper=record->paper;
        index.active=index.count++;
        loadInk();
        record->paper=paper;
        dirty=true;
        if (!saveInk()) {
            index=before;
            loadInk();
            failed();
            return;
        }
    } else {
        const int next=int(index.active)+(action==NotebookAction::Next ? 1 : -1);
        if (next<0 || next>=index.count) return;
        index.active=next;
        loadInk();
    }
    saveIndexes();
}
void savePrefs(prose::Preferences prefs) {
    if (!nvs) return;
    const uint8_t values[]={uint8_t(prefs.font),uint8_t(prefs.dark),uint8_t(prefs.scale),uint8_t(prefs.spacing)};
    kv.putBytes("appearance",values,sizeof(values));
}
void screenChanged(Screen screen) {
    if (screen==Screen::Boot || screen==Screen::Settings || screen==Screen::Templates || screen==Screen::Rename) return;
    if (screen!=Screen::Scribe && annotation>=0) {
        annotate(-1);
    }
    sessionScreen=screen;
    saveSession();
}
int batteryPercent() { return -1; }
} // namespace

prose::Services services() {
    return {library,page,turn,ink,entryCount,entry,openEntry,savePrefs,notebook,notebookAction,
            renamePage,bookmark,annotate,screenChanged,batteryPercent};
}

bool begin(prose::UI& application) {
    ui=&application;
    record=static_cast<InkRecord*>(ps_malloc(sizeof(InkRecord)));
    bookText=static_cast<char*>(ps_malloc(maxBookBytes));
    if (!record || !bookText || !canvas.getBuffer() || !undoCanvas.getBuffer()) return false;
    panel::setInk(&canvas);
    pinMode(board::sdDetect,INPUT_PULLUP);
    cardMounted=digitalRead(board::sdDetect)==LOW &&
                SD_MMC.setPins(board::sdClock,board::sdCmd,board::sdData) &&
                SD_MMC.begin("/sdcard",true,false,10000);
    if (cardMounted && !SD_MMC.exists("/notes")) SD_MMC.mkdir("/notes");
    Serial.printf("microSD %s\n",cardMounted ? "mounted" : "unavailable");
    nvs=kv.begin("dsh-prose",false);
    if (nvs && kv.getBytesLength("notebooks")==sizeof(indexes)) kv.getBytes("notebooks",indexes,sizeof(indexes));
    for (auto& index:indexes) {
        if (!index.count || index.count>maxPages) index={0,1};
        if (index.active>=index.count) index.active=0;
    }
    SavedSession session{};
    session.annotation=-1;
    session.screen=uint8_t(Screen::Library);
    if (nvs && kv.getBytesLength("session")==sizeof(session)) kv.getBytes("session",&session,sizeof(session));
    session.path[sizeof(session.path)-1]=0;
    noteIndex=session.notebook<notebookCount ? session.notebook : 0;
    discover();
    bool found=false;
    for (size_t i=0;i<books.size();++i) if (books[i].path==session.path) { bookIndex=i; found=true; break; }
    openBook(bookIndex);
    if (found && session.screen==uint8_t(Screen::Scribe) && session.annotation>=0 &&
        size_t(session.annotation)<sourceLength) annotation=session.annotation;
    loadInk();
    uint8_t prefs[4]={0,0,1,1};
    if (nvs && kv.getBytesLength("appearance")==sizeof(prefs)) kv.getBytes("appearance",prefs,sizeof(prefs));
    ui->restorePreferences({ReadingFont(prefs[0]),prefs[1]!=0,prefs[2],prefs[3]});
    ui->restoreSession(Screen(session.screen));
    return true;
}
} // namespace documents
