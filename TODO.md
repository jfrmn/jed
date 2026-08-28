# BUGS

[x] scrollbar hover over diagnstic not working
[ ] find and replace -> Missing ProcessTextChange (lsp never recieves replacements)
[x] scrollbar click is funky
[x] short file names don't get displayed correctly
[ ] delete line does not work with multiple cursors
[?] delete line messes up tree sitter highlighting (and i think pasting text too) (FIXED?)
[ ] untab at beginning of produces textChange with 0 operations
[ ] multicursor let's you remove all cursors - leading to a crash
[ ] Opening both a searchbar and the tool panel blocks keyboard input
[ ] using the file search bar from start screen and selecting a file with Ctrl+/Shift+Enter crashes

# MAJOR TASKS

[x] repalce basic parser with regex parser
[x] make settings ini files (DID TOML)
[x] move json to language-server stuff
[-] refactor process so that it no longer spawns a thread; language server should spawn its own thread
[x] refactor glyph run with advanced text shaping
[x] restructure project -> source -> src; dependencies -> deps
[ ] implement commands
[ ] synatx highlighting in scrollbar preview and file preview
[ ] open explorer at current file (maybe a command?)
[.] detect file changes -> watch settings and external files

# MINOR TASKS

[x] print stacktrace in ASSERTS()
[x] add key-binding to select inside brackets
[x] add LogDevVariable() to quickly log out the name + value of a local variable
[ ] refactor animation so that all animations use the same logic
[x] rename TextPostion.column to character
[ ] refactor statu-bar to utilize the new glyph run better (e.g. use draw partial at text pos)
[ ] get rid of OnMouseWheel and OnResize etc.

# BACKLOG

[ ] editorcursorattached: animation for selection just like in prompt
[ ] gotoline: add relatives with +/-
[ ] gotoline: Ctrl + Enter = Scroll and close
[ ] make panels resizeable
[ ] langauge server capabilites could be encoded as bitmask
[ ] own vector and string classes -> auto clean without capacity; clear without calling dtor on elems
[ ] implement backupFileBeforeSaving

## COMMAND IDEAS

* pretty screenshot of selected code
* ruler at cursor
* reset panels size (when panels are resizeable)
* align cursors (multicursor)
* close multiple tabs (to right, to left, others)
* Save all
* switch start and end of selection
* repear a character N times
* insert numbers at multicursor
* trim (left/right)
