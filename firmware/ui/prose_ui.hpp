#pragma once

namespace prose {
constexpr int width = 480;
constexpr int height = 800;

enum class Screen : int { Boot, Library, Reader, Scribe, Settings, Templates, Rename };
enum class Button : int { Prev, Mode, Next };
enum class Font : int { Label, Sans, Serif, Mono, OpenDyslexic, ReaderSerif, ReaderSans, ReaderMono };
enum class Icon : int { Logo, Home, Book, Notebook, Left, Right, Pen, Eraser, Undo, Save, Close, Settings };

enum class ReadingFont : int { Serif, Sans, Mono, OpenDyslexic };
struct Preferences {
    ReadingFont font;
    bool dark;
    int scale;
    int spacing;
    constexpr Preferences(ReadingFont face=ReadingFont::Serif, bool night=false, int size=1, int gap=1)
        : font(face), dark(night), scale(size), spacing(gap) {}
};

int readingSize(Preferences preferences);
int lineStep(Preferences preferences);
Font readingFace(ReadingFont font);

struct Page {
    const char* title;
    const char* chapter;
    const char* const* lines;
    int lineCount;
    int number;
    int count;
    const int* paragraphAnchors;
    const bool* annotated;
    bool bookmarked;
    int bookmarkCount;
};

struct LibraryItems {
    const char* bookTitle;
    const char* bookCoverLine1;
    const char* bookCoverLine2;
    const char* notebookTitle;
    const char* activeNotebookTitle;
};

enum class Filter : int { All, Books, Notebooks };
struct LibraryEntry {
    const char* title;
    Screen destination;
    int id;
};

enum class Paper : int { Blank, Lined, DotGrid, Checklist };
enum class NotebookAction : int { Previous, Next, NewPage, SetTemplate };
struct NotebookPage {
    const char* name;
    int number;
    int count;
    Paper paper;
    bool annotation;
};
enum class BookmarkAction : int { Toggle, Next };

enum class InkAction : int { Begin, Move, End, Undo, Clear, Save, Checkpoint };
struct InkEvent {
    InkAction action;
    int x;
    int y;
    int size;
    bool erase;
};

struct Display {
    void (*fill)(int x, int y, int w, int h, bool black);
    void (*line)(int x1, int y1, int x2, int y2, int size);
    void (*text)(int x, int baseline, int size, Font font, const char* value, bool black);
    void (*icon)(Icon icon, int x, int y, int size, bool black);
    void (*ink)();
    void (*palette)(bool dark);
};
struct Services {
    LibraryItems (*library)();
    Page (*page)(Preferences preferences);
    void (*turn)(int delta);
    void (*ink)(InkEvent event);
    int (*entryCount)(Filter filter);
    LibraryEntry (*entry)(Filter filter, int index);
    bool (*openEntry)(LibraryEntry entry);
    void (*savePreferences)(Preferences preferences);
    NotebookPage (*notebook)();
    void (*notebookAction)(NotebookAction action, int value);
    void (*renamePage)(const char* name);
    void (*bookmark)(BookmarkAction action);
    bool (*annotate)(int paragraphAnchor);
    void (*screenChanged)(Screen screen);
    int (*batteryPercent)(); // Cached measurement, or -1 when unavailable.
};

class UI {
public:
    constexpr explicit UI(Services services) : services_(services) {}
    void show(Screen screen);
    Screen screen() const { return screen_; }
    void button(Button button, bool held);
    void tap(int x, int y);
    void pointer(int phase, int x, int y);
    void scroll(int delta);
    void draw(const Display& display) const;
    void saveResult(bool success);
    void restoreSession(Screen resume);
    void finishBoot();
    void restorePreferences(Preferences preferences);
    void clearMessage() { message_ = nullptr; }

private:
    void setFilter(Filter filter);
    void selectEntry(int index);
    void drawLibrary(const Display& display, int battery) const;
    void drawReader(const Display& display, int battery) const;
    void drawScribe(const Display& display, int battery) const;
    void drawSettings(const Display& display, int battery) const;
    void changePreferences(Preferences preferences);
    void drawTemplates(const Display& display, int battery) const;
    void drawRename(const Display& display, int battery) const;
    void beginRename();
    void nameKey(char key);
    void closeScribe();
    void endStroke();
    void editInk(InkAction action);

    Services services_;
    Screen screen_ = Screen::Boot;
    Screen resume_ = Screen::Library;
    Screen lastMode_ = Screen::Reader;
    Screen returnFromSettings_ = Screen::Library;
    Preferences preferences_{ReadingFont::Serif,false};
    Filter filter_ = Filter::All;
    int libraryOffset_ = 0;
    int touchX_ = 0;
    int touchY_ = 0;
    bool touching_ = false;
    bool dragged_ = false;
    int size_ = 3;
    bool erase_ = false;
    bool drawing_ = false;
    bool confirmClear_ = false;
    const char* message_ = nullptr;
    char name_[25]{};
    int nameLength_ = 0;
};
} // namespace prose
