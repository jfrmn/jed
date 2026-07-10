#include "doctest/doctest.h"
#include "text/text-buffer.hh"
#include "text/text-position.hh"
#include "text/text-change.hh"

#include <string>

//=============================================================================
// Helpers
//=============================================================================

namespace {

// RAII wrapper so every test gets a freshly initialised buffer that is
// cleaned up automatically (TextBuffer disallows copy/assign).
struct ScopedBuffer {
	TextBuffer buffer = {};
	explicit ScopedBuffer(std::string initialText = {}) {
		buffer.Init(std::move(initialText));
	}
	TextBuffer* operator->() { return &buffer; }
	TextBuffer& operator*()  { return buffer; }
};

TextPosition Pos(u64 line, u64 column) {
	return TextPosition {.line = line, .column = column};
}

// Return a line's text as a std::string. GetText() returns a std::string_view,
// which doctest cannot stringify (it only forward-declares std::ostream), so we
// materialise a std::string for readable assertions and failure messages.
std::string LineText(TextBuffer& buffer, u64 line) {
	return std::string(buffer.GetLineAt(line).GetText());
}

} // namespace

//=============================================================================
// Insert tests
//=============================================================================

TEST_SUITE("TextBuffer::Insert") {

	TEST_CASE("Insert into an empty buffer") {
		ScopedBuffer buf {""};
		REQUIRE(buf->GetText() == "");
		REQUIRE(buf->LineCount() == 1);

		buf->Insert(Pos(0, 0), "hello");

		CHECK(buf->GetText() == "hello");
		CHECK(buf->LineCount() == 1);
	}

	TEST_CASE("Insert at the very beginning of a single line") {
		ScopedBuffer buf {"world"};

		buf->Insert(Pos(0, 0), "hello ");

		CHECK(buf->GetText() == "hello world");
		CHECK(buf->LineCount() == 1);
	}

	TEST_CASE("Insert at the very end of a single line") {
		ScopedBuffer buf {"hello"};

		// column == line length == end of the (only, last) line
		buf->Insert(Pos(0, 5), " world");

		CHECK(buf->GetText() == "hello world");
		CHECK(buf->LineCount() == 1);
	}

	TEST_CASE("Insert in the middle of a single line") {
		ScopedBuffer buf {"helloworld"};

		buf->Insert(Pos(0, 5), " ");

		CHECK(buf->GetText() == "hello world");
		CHECK(buf->LineCount() == 1);
	}

	TEST_CASE("Insert an empty string is a no-op for the content") {
		ScopedBuffer buf {"hello"};

		buf->Insert(Pos(0, 3), "");

		CHECK(buf->GetText() == "hello");
		CHECK(buf->LineCount() == 1);
	}

	TEST_CASE("Insert a newline splits a single line into two") {
		ScopedBuffer buf {"helloworld"};

		buf->Insert(Pos(0, 5), "\n");

		CHECK(buf->GetText() == "hello\nworld");
		CHECK(buf->LineCount() == 2);
		CHECK(LineText(*buf, 0) == "hello");
		CHECK(LineText(*buf, 1) == "world");
	}

	TEST_CASE("Insert multi-line text into a single line") {
		ScopedBuffer buf {"ad"};

		buf->Insert(Pos(0, 1), "b\nc");

		CHECK(buf->GetText() == "ab\ncd");
		CHECK(buf->LineCount() == 2);
		CHECK(LineText(*buf, 0) == "ab");
		CHECK(LineText(*buf, 1) == "cd");
	}

	TEST_CASE("Insert at the very beginning of a multi-line buffer") {
		ScopedBuffer buf {"line1\nline2\nline3"};
		REQUIRE(buf->LineCount() == 3);

		buf->Insert(Pos(0, 0), ">> ");

		CHECK(buf->GetText() == ">> line1\nline2\nline3");
		CHECK(buf->LineCount() == 3);
		CHECK(LineText(*buf, 0) == ">> line1");
	}

	TEST_CASE("Insert at the very end of a multi-line buffer (last line)") {
		ScopedBuffer buf {"line1\nline2\nline3"};
		REQUIRE(buf->LineCount() == 3);

		// last line "line3" has no linebreak; its end column is 5
		buf->Insert(Pos(2, 5), " END");

		CHECK(buf->GetText() == "line1\nline2\nline3 END");
		CHECK(buf->LineCount() == 3);
		CHECK(LineText(*buf, 2) == "line3 END");
	}

	TEST_CASE("Insert at the end of a non-last line (before its linebreak)") {
		ScopedBuffer buf {"line1\nline2"};

		// end of "line1" is column 5, right before the '\n'
		buf->Insert(Pos(0, 5), "X");

		CHECK(buf->GetText() == "line1X\nline2");
		CHECK(buf->LineCount() == 2);
		CHECK(LineText(*buf, 0) == "line1X");
		CHECK(LineText(*buf, 1) == "line2");
	}

	TEST_CASE("Insert on a middle line of a multi-line (shared chunk) buffer") {
		ScopedBuffer buf {"aaa\nbbb\nccc"};

		buf->Insert(Pos(1, 1), "XY");

		CHECK(buf->GetText() == "aaa\nbXYbb\nccc");
		CHECK(buf->LineCount() == 3);
	}

	TEST_CASE("Insert a trailing newline at the very end of the buffer") {
		ScopedBuffer buf {"abc"};

		buf->Insert(Pos(0, 3), "\n");

		CHECK(buf->GetText() == "abc\n");
		// an empty final line is created after the linebreak
		CHECK(buf->LineCount() == 2);
		CHECK(LineText(*buf, 0) == "abc");
		CHECK(LineText(*buf, 1) == "");
	}

	TEST_CASE("Sequential inserts accumulate correctly") {
		ScopedBuffer buf {""};

		buf->Insert(Pos(0, 0), "b");
		buf->Insert(Pos(0, 0), "a");   // prepend
		buf->Insert(Pos(0, 2), "c");   // append at end

		CHECK(buf->GetText() == "abc");
		CHECK(buf->LineCount() == 1);
	}

	TEST_CASE("Insert reports the inserted text via TextChangeOperation") {
		ScopedBuffer buf {"hello"};

		TextChangeOperation change = {};
		buf->Insert(Pos(0, 5), " world", &change);

		CHECK(change.start == Pos(0, 5));
		CHECK(change.insertedText == " world");
	}
}

//=============================================================================
// Remove tests
//=============================================================================

TEST_SUITE("TextBuffer::Remove") {

	TEST_CASE("Remove from the very beginning of a single line") {
		ScopedBuffer buf {"hello world"};

		buf->Remove(Pos(0, 0), Pos(0, 6));

		CHECK(buf->GetText() == "world");
		CHECK(buf->LineCount() == 1);
	}

	TEST_CASE("Remove at the very end of a single line") {
		ScopedBuffer buf {"hello world"};

		buf->Remove(Pos(0, 5), Pos(0, 11));

		CHECK(buf->GetText() == "hello");
		CHECK(buf->LineCount() == 1);
	}

	TEST_CASE("Remove in the middle of a single line") {
		ScopedBuffer buf {"hello world"};

		buf->Remove(Pos(0, 5), Pos(0, 6)); // remove the space

		CHECK(buf->GetText() == "helloworld");
		CHECK(buf->LineCount() == 1);
	}

	TEST_CASE("Remove the entire content of a single line") {
		ScopedBuffer buf {"hello"};

		buf->Remove(Pos(0, 0), Pos(0, 5));

		CHECK(buf->GetText() == "");
		CHECK(buf->LineCount() == 1);
	}

	TEST_CASE("Remove a single character at the very beginning") {
		ScopedBuffer buf {"hello"};

		buf->Remove(Pos(0, 0), Pos(0, 1));

		CHECK(buf->GetText() == "ello");
		CHECK(buf->LineCount() == 1);
	}

	TEST_CASE("Remove a single character at the very end") {
		ScopedBuffer buf {"hello"};

		buf->Remove(Pos(0, 4), Pos(0, 5));

		CHECK(buf->GetText() == "hell");
		CHECK(buf->LineCount() == 1);
	}

	TEST_CASE("Remove a linebreak merges two lines") {
		ScopedBuffer buf {"hello\nworld"};
		REQUIRE(buf->LineCount() == 2);

		// from end of line 0 (before '\n') to start of line 1
		buf->Remove(Pos(0, 5), Pos(1, 0));

		CHECK(buf->GetText() == "helloworld");
		CHECK(buf->LineCount() == 1);
		CHECK(LineText(*buf, 0) == "helloworld");
	}

	TEST_CASE("Remove spanning part of two adjacent lines") {
		ScopedBuffer buf {"hello\nworld"};

		// remove "lo\nwo" -> keep "hel" + "rld"
		buf->Remove(Pos(0, 3), Pos(1, 2));

		CHECK(buf->GetText() == "helrld");
		CHECK(buf->LineCount() == 1);
	}

	TEST_CASE("Remove spanning multiple lines") {
		ScopedBuffer buf {"line1\nline2\nline3\nline4"};
		REQUIRE(buf->LineCount() == 4);

		// remove from middle of line1 to middle of line4:
		// keep "li" (line1[0..2)) + "ne4" (line4[2..)) == "line4"
		buf->Remove(Pos(0, 2), Pos(3, 2));

		CHECK(buf->GetText() == "line4");
		CHECK(buf->LineCount() == 1);
	}

	TEST_CASE("Remove whole middle line via a multi-line range") {
		ScopedBuffer buf {"aaa\nbbb\nccc"};

		// remove from end of line 0 to end of line 1 (removes "\nbbb")
		buf->Remove(Pos(0, 3), Pos(1, 3));

		CHECK(buf->GetText() == "aaa\nccc");
		CHECK(buf->LineCount() == 2);
		CHECK(LineText(*buf, 0) == "aaa");
		CHECK(LineText(*buf, 1) == "ccc");
	}

	TEST_CASE("Remove reports the removed text via TextChangeOperation") {
		ScopedBuffer buf {"hello world"};

		TextChangeOperation change = {};
		buf->Remove(Pos(0, 5), Pos(0, 11), &change);

		CHECK(change.start == Pos(0, 5));
		CHECK(change.removalEnd == Pos(0, 11));
		CHECK(change.removedText == " world");
	}

	TEST_CASE("Remove reports removed text spanning a linebreak") {
		ScopedBuffer buf {"hello\nworld"};

		TextChangeOperation change = {};
		buf->Remove(Pos(0, 5), Pos(1, 0), &change);

		CHECK(change.removedText == "\n");
	}
}

//=============================================================================
// Combined Insert + Remove round-trips
//=============================================================================

TEST_SUITE("TextBuffer::InsertRemoveRoundTrip") {

	TEST_CASE("Insert then remove the same range restores the text") {
		ScopedBuffer buf {"hello"};

		buf->Insert(Pos(0, 5), " world");
		REQUIRE(buf->GetText() == "hello world");

		buf->Remove(Pos(0, 5), Pos(0, 11));

		CHECK(buf->GetText() == "hello");
		CHECK(buf->LineCount() == 1);
	}

	TEST_CASE("Insert a newline then remove it restores a single line") {
		ScopedBuffer buf {"helloworld"};

		buf->Insert(Pos(0, 5), "\n");
		REQUIRE(buf->GetText() == "hello\nworld");
		REQUIRE(buf->LineCount() == 2);

		buf->Remove(Pos(0, 5), Pos(1, 0));

		CHECK(buf->GetText() == "helloworld");
		CHECK(buf->LineCount() == 1);
	}
}
