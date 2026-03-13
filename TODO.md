# BUGS

[ ] scrollbar hover over diagnstic not working
[ ] find and replace -> Missing ProcessTextChange (lsp never recieves replacements)
[ ] scrollbar click is funky
[ ] short file names don't get displayed correctly
[ ] delete line does not work with multiple cursors


# MAJOR TASKS

[ ] repalce basic parser with regex parser
[ ] make settings ini files; move json to language-server stuff
[ ] refactor process so that it no longer spawns a thread; language server should spawn its own thread
[x] refactor glyph run with advanced text shaping
[x] restructure project -> source -> src; dependencies -> deps


# MINOR TASKS

[ ] print stacktrace in ASSERTS()
[x] add key-binding to select inside brackets
[x] add LogDevVariable() to quickly log out the name + value of a local variable
[ ] refactor animation so that all animations use the same logic
[ ] rename TextPostion.column to character
[ ] refactor statu-bar to utilize the new glyph run better (e.g. use draw partial at text pos)


# BACKLOG

[ ] editorcursorattached: animation for selection just like in prompt
[ ] gotoline: add relatives with +/-
[ ] gotoline: Ctrl + Enter = Scroll and close
[ ] make panels resizeable
[ ] langauge server capabilites could be encoded as bitmask
[ ] own vector and string classes -> auto clean without capacity; clear without calling dtor on elems


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
