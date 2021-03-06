/**
 * \file Paragraph.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Asger Alstrup
 * \author Lars Gullik Bjønnes
 * \author Richard Heck (XHTML output)
 * \author Jean-Marc Lasgouttes
 * \author Angus Leeming
 * \author John Levon
 * \author André Pönitz
 * \author Dekel Tsur
 * \author Jürgen Vigna
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "Paragraph.h"

#include "LayoutFile.h"
#include "Buffer.h"
#include "BufferParams.h"
#include "Changes.h"
#include "Counters.h"
#include "Encoding.h"
#include "InsetList.h"
#include "Language.h"
#include "LaTeXFeatures.h"
#include "Layout.h"
#include "Length.h"
#include "Font.h"
#include "FontList.h"
#include "LyXRC.h"
#include "OutputParams.h"
#include "output_latex.h"
#include "output_xhtml.h"
#include "ParagraphParameters.h"
#include "SpellChecker.h"
#include "sgml.h"
#include "TextClass.h"
#include "TexRow.h"
#include "Text.h"
#include "VSpace.h"
#include "WordLangTuple.h"
#include "WordList.h"

#include "frontends/alert.h"

#include "insets/InsetBibitem.h"
#include "insets/InsetLabel.h"

#include "support/debug.h"
#include "support/docstring_list.h"
#include "support/ExceptionMessage.h"
#include "support/gettext.h"
#include "support/lassert.h"
#include "support/lstrings.h"
#include "support/textutils.h"

#include <sstream>
#include <vector>

using namespace std;
using namespace lyx::support;

namespace lyx {

namespace {
/// Inset identifier (above 0x10ffff, for ucs-4)
char_type const META_INSET = 0x200001;
};

/////////////////////////////////////////////////////////////////////
//
// Paragraph::Private
//
/////////////////////////////////////////////////////////////////////

class Paragraph::Private
{
public:
	///
	Private(Paragraph * owner, Layout const & layout);
	/// "Copy constructor"
	Private(Private const &, Paragraph * owner);
	/// Copy constructor from \p beg  to \p end
	Private(Private const &, Paragraph * owner, pos_type beg, pos_type end);

	///
	void insertChar(pos_type pos, char_type c, Change const & change);

	/// Output the surrogate pair formed by \p c and \p next to \p os.
	/// \return the number of characters written.
	int latexSurrogatePair(odocstream & os, char_type c, char_type next,
			       OutputParams const &);

	/// Output a space in appropriate formatting (or a surrogate pair
	/// if the next character is a combining character).
	/// \return whether a surrogate pair was output.
	bool simpleTeXBlanks(OutputParams const &,
			     odocstream &, TexRow & texrow,
			     pos_type i,
			     unsigned int & column,
			     Font const & font,
			     Layout const & style);

	/// Output consecutive unicode chars, belonging to the same script as
	/// specified by the latex macro \p ltx, to \p os starting from \p i.
	/// \return the number of characters written.
	int writeScriptChars(odocstream & os, docstring const & ltx,
			   Change const &, Encoding const &, pos_type & i);

	/// This could go to ParagraphParameters if we want to.
	int startTeXParParams(BufferParams const &, odocstream &, TexRow &,
			      OutputParams const &) const;

	/// This could go to ParagraphParameters if we want to.
	int endTeXParParams(BufferParams const &, odocstream &, TexRow &,
			    OutputParams const &) const;

	///
	void latexInset(BufferParams const &,
				   odocstream &,
				   TexRow & texrow, OutputParams &,
				   Font & running_font,
				   Font & basefont,
				   Font const & outerfont,
				   bool & open_font,
				   Change & running_change,
				   Layout const & style,
				   pos_type & i,
				   unsigned int & column);

	///
	void latexSpecialChar(
				   odocstream & os,
				   OutputParams const & runparams,
				   Font const & running_font,
				   Change const & running_change,
				   Layout const & style,
				   pos_type & i,
				   unsigned int & column);

	///
	bool latexSpecialT1(
		char_type const c,
		odocstream & os,
		pos_type i,
		unsigned int & column);
	///
	bool latexSpecialTypewriter(
		char_type const c,
		odocstream & os,
		pos_type i,
		unsigned int & column);
	///
	bool latexSpecialPhrase(
		odocstream & os,
		pos_type & i,
		unsigned int & column,
		OutputParams const & runparams);

	///
	void validate(LaTeXFeatures & features) const;

	/// Checks if the paragraph contains only text and no inset or font change.
	bool onlyText(Buffer const & buf, Font const & outerfont,
		      pos_type initial) const;

	/// match a string against a particular point in the paragraph
	bool isTextAt(string const & str, pos_type pos) const;

	void setMisspelled(pos_type from, pos_type to, bool misspelled)
	{
		pos_type textsize = owner_->size();
		// check for sane arguments
		if (to < from || from >= textsize) return;
		// don't mark end of paragraph
		if (to >= textsize)
			to = textsize - 1;
		fontlist_.setMisspelled(from, to, misspelled);
	}
	

	InsetCode ownerCode() const
	{
		return inset_owner_ ? inset_owner_->lyxCode() : NO_CODE;
	}
	
	/// Which Paragraph owns us?
	Paragraph * owner_;

	/// In which Inset?
	Inset const * inset_owner_;

	///
	FontList fontlist_;

	///
	int id_;

	///
	ParagraphParameters params_;

	/// for recording and looking up changes
	Changes changes_;

	///
	InsetList insetlist_;

	/// end of label
	pos_type begin_of_body_;

	typedef docstring TextContainer;
	///
	TextContainer text_;
	
	typedef set<docstring> Words;
	typedef map<Language, Words> LangWordsMap;
	///
	LangWordsMap words_;
	///
	Layout const * layout_;
};


namespace {

struct special_phrase {
	string phrase;
	docstring macro;
	bool builtin;
};

special_phrase const special_phrases[] = {
	{ "LyX", from_ascii("\\LyX{}"), false },
	{ "TeX", from_ascii("\\TeX{}"), true },
	{ "LaTeX2e", from_ascii("\\LaTeXe{}"), true },
	{ "LaTeX", from_ascii("\\LaTeX{}"), true },
};

size_t const phrases_nr = sizeof(special_phrases)/sizeof(special_phrase);

} // namespace anon


Paragraph::Private::Private(Paragraph * owner, Layout const & layout)
	: owner_(owner), inset_owner_(0), id_(-1), begin_of_body_(0), layout_(&layout)
{
	text_.reserve(100);
}


// Initialization of the counter for the paragraph id's,
//
// FIXME: There should be a more intelligent way to generate and use the
// paragraph ids per buffer instead a global static counter for all InsetText
// in the running program.
static int paragraph_id = -1;

Paragraph::Private::Private(Private const & p, Paragraph * owner)
	: owner_(owner), inset_owner_(p.inset_owner_), fontlist_(p.fontlist_), 
	  params_(p.params_), changes_(p.changes_), insetlist_(p.insetlist_),
	  begin_of_body_(p.begin_of_body_), text_(p.text_), words_(p.words_),
	  layout_(p.layout_)
{
	id_ = ++paragraph_id;
}


Paragraph::Private::Private(Private const & p, Paragraph * owner,
	pos_type beg, pos_type end)
	: owner_(owner), inset_owner_(p.inset_owner_),
	  params_(p.params_), changes_(p.changes_),
	  insetlist_(p.insetlist_, beg, end),
	  begin_of_body_(p.begin_of_body_), words_(p.words_),
	  layout_(p.layout_)
{
	id_ = ++paragraph_id;
	if (beg >= pos_type(p.text_.size()))
		return;
	text_ = p.text_.substr(beg, end - beg);

	FontList::const_iterator fcit = fontlist_.begin();
	FontList::const_iterator fend = fontlist_.end();
	for (; fcit != fend; ++fcit) {
		if (fcit->pos() < beg)
			continue;
		if (fcit->pos() >= end) {
			// Add last entry in the fontlist_.
			fontlist_.set(text_.size() - 1, fcit->font());
			break;
		}
		// Add a new entry in the fontlist_.
		fontlist_.set(fcit->pos() - beg, fcit->font());
	}
}


void Paragraph::addChangesToToc(DocIterator const & cdit,
	Buffer const & buf) const
{
	d->changes_.addToToc(cdit, buf);
}


bool Paragraph::isDeleted(pos_type start, pos_type end) const
{
	LASSERT(start >= 0 && start <= size(), /**/);
	LASSERT(end > start && end <= size() + 1, /**/);

	return d->changes_.isDeleted(start, end);
}


bool Paragraph::isChanged(pos_type start, pos_type end) const
{
	LASSERT(start >= 0 && start <= size(), /**/);
	LASSERT(end > start && end <= size() + 1, /**/);

	return d->changes_.isChanged(start, end);
}


bool Paragraph::isMergedOnEndOfParDeletion(bool trackChanges) const
{
	// keep the logic here in sync with the logic of eraseChars()
	if (!trackChanges)
		return true;

	Change const change = d->changes_.lookup(size());
	return change.inserted() && change.currentAuthor();
}


void Paragraph::setChange(Change const & change)
{
	// beware of the imaginary end-of-par character!
	d->changes_.set(change, 0, size() + 1);

	/*
	 * Propagate the change recursively - but not in case of DELETED!
	 *
	 * Imagine that your co-author makes changes in an existing inset. He
	 * sends your document to you and you come to the conclusion that the
	 * inset should go completely. If you erase it, LyX must not delete all
	 * text within the inset. Otherwise, the change tracked insertions of
	 * your co-author get lost and there is no way to restore them later.
	 *
	 * Conclusion: An inset's content should remain untouched if you delete it
	 */

	if (!change.deleted()) {
		for (pos_type pos = 0; pos < size(); ++pos) {
			if (Inset * inset = getInset(pos))
				inset->setChange(change);
		}
	}
}


void Paragraph::setChange(pos_type pos, Change const & change)
{
	LASSERT(pos >= 0 && pos <= size(), /**/);
	d->changes_.set(change, pos);

	// see comment in setChange(Change const &) above
	if (!change.deleted() && pos < size())
			if (Inset * inset = getInset(pos))
				inset->setChange(change);
}


Change const & Paragraph::lookupChange(pos_type pos) const
{
	LASSERT(pos >= 0 && pos <= size(), /**/);
	return d->changes_.lookup(pos);
}


void Paragraph::acceptChanges(pos_type start, pos_type end)
{
	LASSERT(start >= 0 && start <= size(), /**/);
	LASSERT(end > start && end <= size() + 1, /**/);

	for (pos_type pos = start; pos < end; ++pos) {
		switch (lookupChange(pos).type) {
			case Change::UNCHANGED:
				// accept changes in nested inset
				if (Inset * inset = getInset(pos))
					inset->acceptChanges();
				break;

			case Change::INSERTED:
				d->changes_.set(Change(Change::UNCHANGED), pos);
				// also accept changes in nested inset
				if (Inset * inset = getInset(pos))
					inset->acceptChanges();
				break;

			case Change::DELETED:
				// Suppress access to non-existent
				// "end-of-paragraph char"
				if (pos < size()) {
					eraseChar(pos, false);
					--end;
					--pos;
				}
				break;
		}

	}
}


void Paragraph::rejectChanges(pos_type start, pos_type end)
{
	LASSERT(start >= 0 && start <= size(), /**/);
	LASSERT(end > start && end <= size() + 1, /**/);

	for (pos_type pos = start; pos < end; ++pos) {
		switch (lookupChange(pos).type) {
			case Change::UNCHANGED:
				// reject changes in nested inset
				if (Inset * inset = getInset(pos))
						inset->rejectChanges();
				break;

			case Change::INSERTED:
				// Suppress access to non-existent
				// "end-of-paragraph char"
				if (pos < size()) {
					eraseChar(pos, false);
					--end;
					--pos;
				}
				break;

			case Change::DELETED:
				d->changes_.set(Change(Change::UNCHANGED), pos);

				// Do NOT reject changes within a deleted inset!
				// There may be insertions of a co-author inside of it!

				break;
		}
	}
}


void Paragraph::Private::insertChar(pos_type pos, char_type c,
		Change const & change)
{
	LASSERT(pos >= 0 && pos <= int(text_.size()), /**/);

	// track change
	changes_.insert(change, pos);

	// This is actually very common when parsing buffers (and
	// maybe inserting ascii text)
	if (pos == pos_type(text_.size())) {
		// when appending characters, no need to update tables
		text_.push_back(c);
		return;
	}

	text_.insert(text_.begin() + pos, c);

	// Update the font table.
	fontlist_.increasePosAfterPos(pos);

	// Update the insets
	insetlist_.increasePosAfterPos(pos);
}


bool Paragraph::insertInset(pos_type pos, Inset * inset,
				   Change const & change)
{
	LASSERT(inset, /**/);
	LASSERT(pos >= 0 && pos <= size(), /**/);

	// Paragraph::insertInset() can be used in cut/copy/paste operation where
	// d->inset_owner_ is not set yet.
	if (d->inset_owner_ && !d->inset_owner_->insetAllowed(inset->lyxCode()))
		return false;

	d->insertChar(pos, META_INSET, change);
	LASSERT(d->text_[pos] == META_INSET, /**/);

	// Add a new entry in the insetlist_.
	d->insetlist_.insert(inset, pos);
	return true;
}


bool Paragraph::eraseChar(pos_type pos, bool trackChanges)
{
	LASSERT(pos >= 0 && pos <= size(), return false);

	// keep the logic here in sync with the logic of isMergedOnEndOfParDeletion()

	if (trackChanges) {
		Change change = d->changes_.lookup(pos);

		// set the character to DELETED if
		//  a) it was previously unchanged or
		//  b) it was inserted by a co-author

		if (!change.changed() ||
		      (change.inserted() && !change.currentAuthor())) {
			setChange(pos, Change(Change::DELETED));
			return false;
		}

		if (change.deleted())
			return false;
	}

	// Don't physically access the imaginary end-of-paragraph character.
	// eraseChar() can only mark it as DELETED. A physical deletion of
	// end-of-par must be handled externally.
	if (pos == size()) {
		return false;
	}

	// track change
	d->changes_.erase(pos);

	// if it is an inset, delete the inset entry
	if (d->text_[pos] == META_INSET)
		d->insetlist_.erase(pos);

	d->text_.erase(d->text_.begin() + pos);

	// Update the fontlist_
	d->fontlist_.erase(pos);

	// Update the insetlist_
	d->insetlist_.decreasePosAfterPos(pos);

	return true;
}


int Paragraph::eraseChars(pos_type start, pos_type end, bool trackChanges)
{
	LASSERT(start >= 0 && start <= size(), /**/);
	LASSERT(end >= start && end <= size() + 1, /**/);

	pos_type i = start;
	for (pos_type count = end - start; count; --count) {
		if (!eraseChar(i, trackChanges))
			++i;
	}
	return end - i;
}


int Paragraph::Private::latexSurrogatePair(odocstream & os, char_type c,
		char_type next, OutputParams const & runparams)
{
	// Writing next here may circumvent a possible font change between
	// c and next. Since next is only output if it forms a surrogate pair
	// with c we can ignore this:
	// A font change inside a surrogate pair does not make sense and is
	// hopefully impossible to input.
	// FIXME: change tracking
	// Is this correct WRT change tracking?
	Encoding const & encoding = *(runparams.encoding);
	docstring const latex1 = encoding.latexChar(next);
	docstring const latex2 = encoding.latexChar(c);
	if (docstring(1, next) == latex1) {
		// the encoding supports the combination
		os << latex2 << latex1;
		return latex1.length() + latex2.length();
	} else if (runparams.local_font &&
		   runparams.local_font->language()->lang() == "polutonikogreek") {
		// polutonikogreek only works without the brackets
		os << latex1 << latex2;
		return latex1.length() + latex2.length();
	} else
		os << latex1 << '{' << latex2 << '}';
	return latex1.length() + latex2.length() + 2;
}


bool Paragraph::Private::simpleTeXBlanks(OutputParams const & runparams,
				       odocstream & os, TexRow & texrow,
				       pos_type i,
				       unsigned int & column,
				       Font const & font,
				       Layout const & style)
{
	if (style.pass_thru || runparams.pass_thru)
		return false;

	if (i + 1 < int(text_.size())) {
		char_type next = text_[i + 1];
		if (Encodings::isCombiningChar(next)) {
			// This space has an accent, so we must always output it.
			column += latexSurrogatePair(os, ' ', next, runparams) - 1;
			return true;
		}
	}

	if (runparams.linelen > 0
	    && column > runparams.linelen
	    && i
	    && text_[i - 1] != ' '
	    && (i + 1 < int(text_.size()))
	    // same in FreeSpacing mode
	    && !owner_->isFreeSpacing()
	    // In typewriter mode, we want to avoid
	    // ! . ? : at the end of a line
	    && !(font.fontInfo().family() == TYPEWRITER_FAMILY
		 && (text_[i - 1] == '.'
		     || text_[i - 1] == '?'
		     || text_[i - 1] == ':'
		     || text_[i - 1] == '!'))) {
		os << '\n';
		texrow.newline();
		texrow.start(owner_->id(), i + 1);
		column = 0;
	} else if (style.free_spacing) {
		os << '~';
	} else {
		os << ' ';
	}
	return false;
}


int Paragraph::Private::writeScriptChars(odocstream & os,
					 docstring const & ltx,
					 Change const & runningChange,
					 Encoding const & encoding,
					 pos_type & i)
{
	// FIXME: modifying i here is not very nice...

	// We only arrive here when a proper language for character text_[i] has
	// not been specified (i.e., it could not be translated in the current
	// latex encoding) or its latex translation has been forced, and it
	// belongs to a known script.
	// Parameter ltx contains the latex translation of text_[i] as specified
	// in the unicodesymbols file and is something like "\textXXX{<spec>}".
	// The latex macro name "textXXX" specifies the script to which text_[i]
	// belongs and we use it in order to check whether characters from the
	// same script immediately follow, such that we can collect them in a
	// single "\textXXX" macro. So, we have to retain "\textXXX{<spec>"
	// for the first char but only "<spec>" for all subsequent chars.
	docstring::size_type const brace1 = ltx.find_first_of(from_ascii("{"));
	docstring::size_type const brace2 = ltx.find_last_of(from_ascii("}"));
	string script = to_ascii(ltx.substr(1, brace1 - 1));
	int pos = 0;
	int length = brace2;
	bool closing_brace = true;
	if (script == "textgreek" && encoding.latexName() == "iso-8859-7") {
		// Correct encoding is being used, so we can avoid \textgreek.
		pos = brace1 + 1;
		length -= pos;
		closing_brace = false;
	}
	os << ltx.substr(pos, length);
	int size = text_.size();
	while (i + 1 < size) {
		char_type const next = text_[i + 1];
		// Stop here if next character belongs to another script
		// or there is a change in change tracking status.
		if (!Encodings::isKnownScriptChar(next, script) ||
		    runningChange != owner_->lookupChange(i + 1))
			break;
		Font prev_font;
		bool found = false;
		FontList::const_iterator cit = fontlist_.begin();
		FontList::const_iterator end = fontlist_.end();
		for (; cit != end; ++cit) {
			if (cit->pos() >= i && !found) {
				prev_font = cit->font();
				found = true;
			}
			if (cit->pos() >= i + 1)
				break;
		}
		// Stop here if there is a font attribute or encoding change.
		if (found && cit != end && prev_font != cit->font())
			break;
		docstring const latex = encoding.latexChar(next);
		docstring::size_type const b1 =
					latex.find_first_of(from_ascii("{"));
		docstring::size_type const b2 =
					latex.find_last_of(from_ascii("}"));
		int const len = b2 - b1 - 1;
		os << latex.substr(b1 + 1, len);
		length += len;
		++i;
	}
	if (closing_brace) {
		os << '}';
		++length;
	}
	return length;
}


bool Paragraph::Private::isTextAt(string const & str, pos_type pos) const
{
	pos_type const len = str.length();

	// is the paragraph large enough?
	if (pos + len > int(text_.size()))
		return false;

	// does the wanted text start at point?
	for (string::size_type i = 0; i < str.length(); ++i) {
		// Caution: direct comparison of characters works only
		// because str is pure ASCII.
		if (str[i] != text_[pos + i])
			return false;
	}

	return fontlist_.hasChangeInRange(pos, len);
}


void Paragraph::Private::latexInset(BufferParams const & bparams,
				    odocstream & os,
				    TexRow & texrow,
				    OutputParams & runparams,
				    Font & running_font,
				    Font & basefont,
				    Font const & outerfont,
				    bool & open_font,
				    Change & running_change,
				    Layout const & style,
				    pos_type & i,
				    unsigned int & column)
{
	Inset * inset = owner_->getInset(i);
	LASSERT(inset, /**/);

	if (style.pass_thru) {
		inset->plaintext(os, runparams);
		return;
	}

	// FIXME: move this to InsetNewline::latex
	if (inset->lyxCode() == NEWLINE_CODE) {
		// newlines are handled differently here than
		// the default in simpleTeXSpecialChars().
		if (!style.newline_allowed) {
			os << '\n';
		} else {
			if (open_font) {
				column += running_font.latexWriteEndChanges(
					os, bparams, runparams,
					basefont, basefont);
				open_font = false;
			}

			if (running_font.fontInfo().family() == TYPEWRITER_FAMILY)
				os << '~';

			basefont = owner_->getLayoutFont(bparams, outerfont);
			running_font = basefont;

			if (runparams.moving_arg)
				os << "\\protect ";

		}
		texrow.newline();
		texrow.start(owner_->id(), i + 1);
		column = 0;
	}

	if (owner_->isDeleted(i)) {
		if( ++runparams.inDeletedInset == 1)
			runparams.changeOfDeletedInset = owner_->lookupChange(i);
	}

	if (inset->canTrackChanges()) {
		column += Changes::latexMarkChange(os, bparams, running_change,
			Change(Change::UNCHANGED), runparams);
		running_change = Change(Change::UNCHANGED);
	}

	bool close = false;
	odocstream::pos_type const len = os.tellp();

	if (inset->forceLTR()
	    && running_font.isRightToLeft()
	    // ERT is an exception, it should be output with no
	    // decorations at all
	    && inset->lyxCode() != ERT_CODE) {
	    	if (running_font.language()->lang() == "farsi")
			os << "\\beginL{}";
		else
			os << "\\L{";
		close = true;
	}

	// FIXME: Bug: we can have an empty font change here!
	// if there has just been a font change, we are going to close it
	// right now, which means stupid latex code like \textsf{}. AFAIK,
	// this does not harm dvi output. A minor bug, thus (JMarc)

	// Some insets cannot be inside a font change command.
	// However, even such insets *can* be placed in \L or \R
	// or their equivalents (for RTL language switches), so we don't
	// close the language in those cases.
	// ArabTeX, though, cannot handle this special behavior, it seems.
	bool arabtex = basefont.language()->lang() == "arabic_arabtex"
		|| running_font.language()->lang() == "arabic_arabtex";
	if (open_font && inset->noFontChange()) {
		bool closeLanguage = arabtex
			|| basefont.isRightToLeft() == running_font.isRightToLeft();
		unsigned int count = running_font.latexWriteEndChanges(os,
			bparams, runparams, basefont, basefont, closeLanguage);
		column += count;
		// if any font properties were closed, update the running_font, 
		// making sure, however, to leave the language as it was
		if (count > 0) {
			// FIXME: probably a better way to keep track of the old 
			// language, than copying the entire font?
			Font const copy_font(running_font);
			basefont = owner_->getLayoutFont(bparams, outerfont);
			running_font = basefont;
			if (!closeLanguage)
				running_font.setLanguage(copy_font.language());
			// leave font open if language is still open
			open_font = (running_font.language() == basefont.language());
			if (closeLanguage)
				runparams.local_font = &basefont;
		}
	}

	int tmp;

	try {
		tmp = inset->latex(os, runparams);
	} catch (EncodingException & e) {
		// add location information and throw again.
		e.par_id = id_;
		e.pos = i;
		throw(e);
	}

	if (close) {
		if (running_font.language()->lang() == "farsi")
				os << "\\endL{}";
			else
				os << '}';
	}

	if (tmp) {
		texrow.newlines(tmp);
		texrow.start(owner_->id(), i + 1);
		column = 0;
	} else {
		column += os.tellp() - len;
	}

	if (owner_->isDeleted(i))
		--runparams.inDeletedInset;
}


void Paragraph::Private::latexSpecialChar(
					     odocstream & os,
					     OutputParams const & runparams,
					     Font const & running_font,
					     Change const & running_change,
					     Layout const & style,
					     pos_type & i,
					     unsigned int & column)
{
	char_type const c = text_[i];

	if (style.pass_thru || runparams.pass_thru) {
		if (c != '\0')
			// FIXME UNICODE: This can fail if c cannot
			// be encoded in the current encoding.
			os.put(c);
		return;
	}

	// If T1 font encoding is used, use the special
	// characters it provides.
	// NOTE: some languages reset the font encoding
	// internally
	if (!running_font.language()->internalFontEncoding()
	    && lyxrc.fontenc == "T1" && latexSpecialT1(c, os, i, column))
		return;

	// \tt font needs special treatment
	if (running_font.fontInfo().family() == TYPEWRITER_FAMILY
		&& latexSpecialTypewriter(c, os, i, column))
		return;

	// Otherwise, we use what LaTeX provides us.
	switch (c) {
	case '\\':
		os << "\\textbackslash{}";
		column += 15;
		break;
	case '<':
		os << "\\textless{}";
		column += 10;
		break;
	case '>':
		os << "\\textgreater{}";
		column += 13;
		break;
	case '|':
		os << "\\textbar{}";
		column += 9;
		break;
	case '-':
		os << '-';
		break;
	case '\"':
		os << "\\char`\\\"{}";
		column += 9;
		break;

	case '$': case '&':
	case '%': case '#': case '{':
	case '}': case '_':
		os << '\\';
		os.put(c);
		column += 1;
		break;

	case '~':
		os << "\\textasciitilde{}";
		column += 16;
		break;

	case '^':
		os << "\\textasciicircum{}";
		column += 17;
		break;

	case '*':
	case '[':
	case ']':
		// avoid being mistaken for optional arguments
		os << '{';
		os.put(c);
		os << '}';
		column += 2;
		break;

	case ' ':
		// Blanks are printed before font switching.
		// Sure? I am not! (try nice-latex)
		// I am sure it's correct. LyX might be smarter
		// in the future, but for now, nothing wrong is
		// written. (Asger)
		break;

	default:
		// LyX, LaTeX etc.
		if (latexSpecialPhrase(os, i, column, runparams))
			return;

		if (c == '\0')
			return;

		Encoding const & encoding = *(runparams.encoding);
		if (i + 1 < int(text_.size())) {
			char_type next = text_[i + 1];
			if (Encodings::isCombiningChar(next)) {
				column += latexSurrogatePair(os, c, next, runparams) - 1;
				++i;
				break;
			}
		}
		string script;
		docstring const latex = encoding.latexChar(c);
		if (Encodings::isKnownScriptChar(c, script)
		    && prefixIs(latex, from_ascii("\\" + script)))
			column += writeScriptChars(os, latex,
					running_change, encoding, i) - 1;
		else if (latex.length() > 1 && latex[latex.length() - 1] != '}') {
			// Prevent eating of a following
			// space or command corruption by
			// following characters
			column += latex.length() + 1;
			os << latex << "{}";
		} else {
			column += latex.length() - 1;
			os << latex;
		}
		break;
	}
}


bool Paragraph::Private::latexSpecialT1(char_type const c, odocstream & os,
	pos_type i, unsigned int & column)
{
	switch (c) {
	case '>':
	case '<':
		os.put(c);
		// In T1 encoding, these characters exist
		// but we should avoid ligatures
		if (i + 1 >= int(text_.size()) || text_[i + 1] != c)
			return true;
		os << "\\textcompwordmark{}";
		column += 19;
		return true;
	case '|':
		os.put(c);
		return true;
	case '\"':
		// soul.sty breaks with \char`\"
		os << "\\textquotedbl{}";
		column += 14;
		return true;
	default:
		return false;
	}
}


bool Paragraph::Private::latexSpecialTypewriter(char_type const c, odocstream & os,
	pos_type i, unsigned int & column)
{
	switch (c) {
	case '-':
		// within \ttfamily, "--" is merged to "-" (no endash)
		// so we avoid this rather irritating ligature
		if (i + 1 < int(text_.size()) && text_[i + 1] == '-') {
			os << "-{}";
			column += 2;
		} else
			os << '-';
		return true;

	// everything else has to be checked separately
	// (depending on the encoding)
	default:
		return false;
	}
}


bool Paragraph::Private::latexSpecialPhrase(odocstream & os, pos_type & i,
	unsigned int & column, OutputParams const & runparams)
{
	// FIXME: if we have "LaTeX" with a font
	// change in the middle (before the 'T', then
	// the "TeX" part is still special cased.
	// Really we should only operate this on
	// "words" for some definition of word

	for (size_t pnr = 0; pnr < phrases_nr; ++pnr) {
		if (!isTextAt(special_phrases[pnr].phrase, i))
			continue;
		if (runparams.moving_arg)
			os << "\\protect";
		os << special_phrases[pnr].macro;
		i += special_phrases[pnr].phrase.length() - 1;
		column += special_phrases[pnr].macro.length() - 1;
		return true;
	}
	return false;
}


void Paragraph::Private::validate(LaTeXFeatures & features) const
{
	if (layout_->inpreamble && inset_owner_) {
		Buffer const & buf = inset_owner_->buffer();
		BufferParams const & bp = buf.params();
		Font f;
		TexRow tr;
		odocstringstream ods;
		// we have to provide all the optional arguments here, even though
		// the last one is the only one we care about.
		owner_->latex(bp, f, ods, tr, features.runparams(), 0, -1, true);
		docstring const d = ods.str();
		if (!d.empty()) {
			// this will have "{" at the beginning, but not at the end
			string const content = to_utf8(d);
			string const cmd = layout_->latexname();
			features.addPreambleSnippet("\\" + cmd + content + "}");
		}
	}
	
	if (features.runparams().flavor == OutputParams::HTML 
	    && layout_->htmltitle()) {
		features.setHTMLTitle(owner_->asString(AS_STR_INSETS));
	}
	
	// check the params.
	if (!params_.spacing().isDefault())
		features.require("setspace");

	// then the layouts
	features.useLayout(layout_->name());

	// then the fonts
	fontlist_.validate(features);

	// then the indentation
	if (!params_.leftIndent().zero())
		features.require("ParagraphLeftIndent");

	// then the insets
	InsetList::const_iterator icit = insetlist_.begin();
	InsetList::const_iterator iend = insetlist_.end();
	for (; icit != iend; ++icit) {
		if (icit->inset) {
			icit->inset->validate(features);
			if (layout_->needprotect &&
			    icit->inset->lyxCode() == FOOT_CODE)
				features.require("NeedLyXFootnoteCode");
		}
	}

	// then the contents
	for (pos_type i = 0; i < int(text_.size()) ; ++i) {
		for (size_t pnr = 0; pnr < phrases_nr; ++pnr) {
			if (!special_phrases[pnr].builtin
			    && isTextAt(special_phrases[pnr].phrase, i)) {
				features.require(special_phrases[pnr].phrase);
				break;
			}
		}
		Encodings::validate(text_[i], features);
	}
}

/////////////////////////////////////////////////////////////////////
//
// Paragraph
//
/////////////////////////////////////////////////////////////////////

namespace {
	Layout const emptyParagraphLayout;
}

Paragraph::Paragraph() 
	: d(new Paragraph::Private(this, emptyParagraphLayout))
{
	itemdepth = 0;
	d->params_.clear();
}


Paragraph::Paragraph(Paragraph const & par)
	: itemdepth(par.itemdepth),
	d(new Paragraph::Private(*par.d, this))
{
	registerWords();
}


Paragraph::Paragraph(Paragraph const & par, pos_type beg, pos_type end)
	: itemdepth(par.itemdepth),
	d(new Paragraph::Private(*par.d, this, beg, end))
{
	registerWords();
}


Paragraph & Paragraph::operator=(Paragraph const & par)
{
	// needed as we will destroy the private part before copying it
	if (&par != this) {
		itemdepth = par.itemdepth;

		deregisterWords();
		delete d;
		d = new Private(*par.d, this);
		registerWords();
	}
	return *this;
}


Paragraph::~Paragraph()
{
	deregisterWords();
	delete d;
}


namespace {

// this shall be called just before every "os << ..." action.
void flushString(ostream & os, docstring & s)
{
	os << to_utf8(s);
	s.erase();
}

}


void Paragraph::write(ostream & os, BufferParams const & bparams,
	depth_type & dth) const
{
	// The beginning or end of a deeper (i.e. nested) area?
	if (dth != d->params_.depth()) {
		if (d->params_.depth() > dth) {
			while (d->params_.depth() > dth) {
				os << "\n\\begin_deeper";
				++dth;
			}
		} else {
			while (d->params_.depth() < dth) {
				os << "\n\\end_deeper";
				--dth;
			}
		}
	}

	// First write the layout
	os << "\n\\begin_layout " << to_utf8(d->layout_->name()) << '\n';

	d->params_.write(os);

	Font font1(inherit_font, bparams.language);

	Change running_change = Change(Change::UNCHANGED);

	// this string is used as a buffer to avoid repetitive calls
	// to to_utf8(), which turn out to be expensive (JMarc)
	docstring write_buffer;

	int column = 0;
	for (pos_type i = 0; i <= size(); ++i) {

		Change const change = lookupChange(i);
		if (change != running_change)
			flushString(os, write_buffer);
		Changes::lyxMarkChange(os, bparams, column, running_change, change);
		running_change = change;

		if (i == size())
			break;

		// Write font changes (ignore spelling markers)
		Font font2 = getFontSettings(bparams, i);
		font2.setMisspelled(false);
		if (font2 != font1) {
			flushString(os, write_buffer);
			font2.lyxWriteChanges(font1, os);
			column = 0;
			font1 = font2;
		}

		char_type const c = d->text_[i];
		switch (c) {
		case META_INSET:
			if (Inset const * inset = getInset(i)) {
				flushString(os, write_buffer);
				if (inset->directWrite()) {
					// international char, let it write
					// code directly so it's shorter in
					// the file
					inset->write(os);
				} else {
					if (i)
						os << '\n';
					os << "\\begin_inset ";
					inset->write(os);
					os << "\n\\end_inset\n\n";
					column = 0;
				}
			}
			break;
		case '\\':
			flushString(os, write_buffer);
			os << "\n\\backslash\n";
			column = 0;
			break;
		case '.':
			flushString(os, write_buffer);
			if (i + 1 < size() && d->text_[i + 1] == ' ') {
				os << ".\n";
				column = 0;
			} else
				os << '.';
			break;
		default:
			if ((column > 70 && c == ' ')
			    || column > 79) {
				flushString(os, write_buffer);
				os << '\n';
				column = 0;
			}
			// this check is to amend a bug. LyX sometimes
			// inserts '\0' this could cause problems.
			if (c != '\0')
				write_buffer.push_back(c);
			else
				LYXERR0("NUL char in structure.");
			++column;
			break;
		}
	}

	flushString(os, write_buffer);
	os << "\n\\end_layout\n";
}


void Paragraph::validate(LaTeXFeatures & features) const
{
	d->validate(features);
}


void Paragraph::insert(pos_type start, docstring const & str,
		       Font const & font, Change const & change)
{
	for (size_t i = 0, n = str.size(); i != n ; ++i)
		insertChar(start + i, str[i], font, change);
}


void Paragraph::appendChar(char_type c, Font const & font,
		Change const & change)
{
	// track change
	d->changes_.insert(change, d->text_.size());
	// when appending characters, no need to update tables
	d->text_.push_back(c);
	setFont(d->text_.size() - 1, font);
}


void Paragraph::appendString(docstring const & s, Font const & font,
		Change const & change)
{
	pos_type end = s.size();
	size_t oldsize = d->text_.size();
	size_t newsize = oldsize + end;
	size_t capacity = d->text_.capacity();
	if (newsize >= capacity)
		d->text_.reserve(max(capacity + 100, newsize));

	// when appending characters, no need to update tables
	d->text_.append(s);

	// FIXME: Optimize this!
	for (size_t i = oldsize; i != newsize; ++i) {
		// track change
		d->changes_.insert(change, i);
	}
	d->fontlist_.set(oldsize, font);
	d->fontlist_.set(newsize - 1, font);
}


void Paragraph::insertChar(pos_type pos, char_type c,
			   bool trackChanges)
{
	d->insertChar(pos, c, Change(trackChanges ?
			   Change::INSERTED : Change::UNCHANGED));
}


void Paragraph::insertChar(pos_type pos, char_type c,
			   Font const & font, bool trackChanges)
{
	d->insertChar(pos, c, Change(trackChanges ?
			   Change::INSERTED : Change::UNCHANGED));
	setFont(pos, font);
}


void Paragraph::insertChar(pos_type pos, char_type c,
			   Font const & font, Change const & change)
{
	d->insertChar(pos, c, change);
	setFont(pos, font);
}


bool Paragraph::insertInset(pos_type pos, Inset * inset,
			    Font const & font, Change const & change)
{
	bool const success = insertInset(pos, inset, change);
	// Set the font/language of the inset...
	setFont(pos, font);
	return success;
}


void Paragraph::resetFonts(Font const & font)
{
	d->fontlist_.clear();
	d->fontlist_.set(0, font);
	d->fontlist_.set(d->text_.size() - 1, font);
}

// Gets uninstantiated font setting at position.
Font const & Paragraph::getFontSettings(BufferParams const & bparams,
					 pos_type pos) const
{
	if (pos > size()) {
		LYXERR0("pos: " << pos << " size: " << size());
		LASSERT(pos <= size(), /**/);
	}

	FontList::const_iterator cit = d->fontlist_.fontIterator(pos);
	if (cit != d->fontlist_.end())
		return cit->font();

	if (pos == size() && !empty())
		return getFontSettings(bparams, pos - 1);

	// Optimisation: avoid a full font instantiation if there is no
	// language change from previous call.
	static Font previous_font;
	static Language const * previous_lang = 0;
	Language const * lang = getParLanguage(bparams);
	if (lang != previous_lang) {
		previous_lang = lang;
		previous_font = Font(inherit_font, lang);
	}
	return previous_font;
}


FontSpan Paragraph::fontSpan(pos_type pos) const
{
	LASSERT(pos <= size(), /**/);
	pos_type start = 0;

	FontList::const_iterator cit = d->fontlist_.begin();
	FontList::const_iterator end = d->fontlist_.end();
	for (; cit != end; ++cit) {
		if (cit->pos() >= pos) {
			if (pos >= beginOfBody())
				return FontSpan(max(start, beginOfBody()),
						cit->pos());
			else
				return FontSpan(start,
						min(beginOfBody() - 1,
							 cit->pos()));
		}
		start = cit->pos() + 1;
	}

	// This should not happen, but if so, we take no chances.
	// LYXERR0("Paragraph::getEndPosOfFontSpan: This should not happen!");
	return FontSpan(pos, pos);
}


// Gets uninstantiated font setting at position 0
Font const & Paragraph::getFirstFontSettings(BufferParams const & bparams) const
{
	if (!empty() && !d->fontlist_.empty())
		return d->fontlist_.begin()->font();

	// Optimisation: avoid a full font instantiation if there is no
	// language change from previous call.
	static Font previous_font;
	static Language const * previous_lang = 0;
	if (bparams.language != previous_lang) {
		previous_lang = bparams.language;
		previous_font = Font(inherit_font, bparams.language);
	}

	return previous_font;
}


// Gets the fully instantiated font at a given position in a paragraph
// This is basically the same function as Text::GetFont() in text2.cpp.
// The difference is that this one is used for generating the LaTeX file,
// and thus cosmetic "improvements" are disallowed: This has to deliver
// the true picture of the buffer. (Asger)
Font const Paragraph::getFont(BufferParams const & bparams, pos_type pos,
				 Font const & outerfont) const
{
	LASSERT(pos >= 0, /**/);

	Font font = getFontSettings(bparams, pos);

	pos_type const body_pos = beginOfBody();
	FontInfo & fi = font.fontInfo();
	if (pos < body_pos)
		fi.realize(d->layout_->labelfont);
	else
		fi.realize(d->layout_->font);

	fi.realize(outerfont.fontInfo());
	fi.realize(bparams.getFont().fontInfo());

	return font;
}


Font const Paragraph::getLabelFont
	(BufferParams const & bparams, Font const & outerfont) const
{
	FontInfo tmpfont = d->layout_->labelfont;
	tmpfont.realize(outerfont.fontInfo());
	tmpfont.realize(bparams.getFont().fontInfo());
	return Font(tmpfont, getParLanguage(bparams));
}


Font const Paragraph::getLayoutFont
	(BufferParams const & bparams, Font const & outerfont) const
{
	FontInfo tmpfont = d->layout_->font;
	tmpfont.realize(outerfont.fontInfo());
	tmpfont.realize(bparams.getFont().fontInfo());
	return Font(tmpfont, getParLanguage(bparams));
}


/// Returns the height of the highest font in range
FontSize Paragraph::highestFontInRange
	(pos_type startpos, pos_type endpos, FontSize def_size) const
{
	return d->fontlist_.highestInRange(startpos, endpos, def_size);
}


char_type Paragraph::getUChar(BufferParams const & bparams, pos_type pos) const
{
	char_type c = d->text_[pos];
	if (!lyxrc.rtl_support)
		return c;

	char_type uc = c;
	switch (c) {
	case '(':
		uc = ')';
		break;
	case ')':
		uc = '(';
		break;
	case '[':
		uc = ']';
		break;
	case ']':
		uc = '[';
		break;
	case '{':
		uc = '}';
		break;
	case '}':
		uc = '{';
		break;
	case '<':
		uc = '>';
		break;
	case '>':
		uc = '<';
		break;
	}
	if (uc != c && getFontSettings(bparams, pos).isRightToLeft())
		return uc;
	return c;
}


void Paragraph::setFont(pos_type pos, Font const & font)
{
	LASSERT(pos <= size(), /**/);

	// First, reduce font against layout/label font
	// Update: The setCharFont() routine in text2.cpp already
	// reduces font, so we don't need to do that here. (Asger)
	
	d->fontlist_.set(pos, font);
}


void Paragraph::makeSameLayout(Paragraph const & par)
{
	d->layout_ = par.d->layout_;
	d->params_ = par.d->params_;
}


bool Paragraph::stripLeadingSpaces(bool trackChanges)
{
	if (isFreeSpacing())
		return false;

	int pos = 0;
	int count = 0;

	while (pos < size() && (isNewline(pos) || isLineSeparator(pos))) {
		if (eraseChar(pos, trackChanges))
			++count;
		else
			++pos;
	}

	return count > 0 || pos > 0;
}


bool Paragraph::hasSameLayout(Paragraph const & par) const
{
	return par.d->layout_ == d->layout_
		&& d->params_.sameLayout(par.d->params_);
}


depth_type Paragraph::getDepth() const
{
	return d->params_.depth();
}


depth_type Paragraph::getMaxDepthAfter() const
{
	if (d->layout_->isEnvironment())
		return d->params_.depth() + 1;
	else
		return d->params_.depth();
}


char Paragraph::getAlign() const
{
	if (d->params_.align() == LYX_ALIGN_LAYOUT)
		return d->layout_->align;
	else
		return d->params_.align();
}


docstring const & Paragraph::labelString() const
{
	return d->params_.labelString();
}


// the next two functions are for the manual labels
docstring const Paragraph::getLabelWidthString() const
{
	if (d->layout_->margintype == MARGIN_MANUAL
	    || d->layout_->latextype == LATEX_BIB_ENVIRONMENT)
		return d->params_.labelWidthString();
	else
		return _("Senseless with this layout!");
}


void Paragraph::setLabelWidthString(docstring const & s)
{
	d->params_.labelWidthString(s);
}


docstring Paragraph::expandLabel(Layout const & layout, 
		BufferParams const & bparams) const
{ 
	return expandParagraphLabel(layout, bparams, true); 
}


docstring Paragraph::expandDocBookLabel(Layout const & layout, 
		BufferParams const & bparams) const
{
	return expandParagraphLabel(layout, bparams, false);
}


docstring Paragraph::expandParagraphLabel(Layout const & layout,
		BufferParams const & bparams, bool process_appendix) const
{
	DocumentClass const & tclass = bparams.documentClass();
	string const & lang = getParLanguage(bparams)->code();
	bool const in_appendix = process_appendix && d->params_.appendix();
	docstring fmt = translateIfPossible(layout.labelstring(in_appendix), lang);

	if (fmt.empty() && layout.labeltype == LABEL_COUNTER 
	    && !layout.counter.empty())
		return tclass.counters().theCounter(layout.counter, lang);

	// handle 'inherited level parts' in 'fmt',
	// i.e. the stuff between '@' in   '@Section@.\arabic{subsection}'
	size_t const i = fmt.find('@', 0);
	if (i != docstring::npos) {
		size_t const j = fmt.find('@', i + 1);
		if (j != docstring::npos) {
			docstring parent(fmt, i + 1, j - i - 1);
			docstring label = from_ascii("??");
			if (tclass.hasLayout(parent))
				docstring label = expandParagraphLabel(tclass[parent], bparams,
						      process_appendix);
			fmt = docstring(fmt, 0, i) + label 
				+ docstring(fmt, j + 1, docstring::npos);
		}
	}

	return tclass.counters().counterLabel(fmt, lang);
}


void Paragraph::applyLayout(Layout const & new_layout)
{
	d->layout_ = &new_layout;
	LyXAlignment const oldAlign = d->params_.align();
	
	if (!(oldAlign & d->layout_->alignpossible)) {
		frontend::Alert::warning(_("Alignment not permitted"), 
			_("The new layout does not permit the alignment previously used.\nSetting to default."));
		d->params_.align(LYX_ALIGN_LAYOUT);
	}
}


pos_type Paragraph::beginOfBody() const
{
	return d->begin_of_body_;
}


void Paragraph::setBeginOfBody()
{
	if (d->layout_->labeltype != LABEL_MANUAL) {
		d->begin_of_body_ = 0;
		return;
	}

	// Unroll the first two cycles of the loop
	// and remember the previous character to
	// remove unnecessary getChar() calls
	pos_type i = 0;
	pos_type end = size();
	if (i < end && !isNewline(i)) {
		++i;
		char_type previous_char = 0;
		char_type temp = 0;
		if (i < end) {
			previous_char = d->text_[i];
			if (!isNewline(i)) {
				++i;
				while (i < end && previous_char != ' ') {
					temp = d->text_[i];
					if (isNewline(i))
						break;
					++i;
					previous_char = temp;
				}
			}
		}
	}

	d->begin_of_body_ = i;
}


bool Paragraph::allowParagraphCustomization() const
{
	return inInset().allowParagraphCustomization();
}


bool Paragraph::usePlainLayout() const
{
	return inInset().usePlainLayout();
}


namespace {

// paragraphs inside floats need different alignment tags to avoid
// unwanted space

bool noTrivlistCentering(InsetCode code)
{
	return code == FLOAT_CODE
	       || code == WRAP_CODE
	       || code == CELL_CODE;
}


string correction(string const & orig)
{
	if (orig == "flushleft")
		return "raggedright";
	if (orig == "flushright")
		return "raggedleft";
	if (orig == "center")
		return "centering";
	return orig;
}


string const corrected_env(string const & suffix, string const & env,
	InsetCode code, bool const lastpar)
{
	string output = suffix + "{";
	if (noTrivlistCentering(code)) {
		if (lastpar) {
			// the last paragraph in non-trivlist-aligned
			// context is special (to avoid unwanted whitespace)
			if (suffix == "\\begin")
				return "\\" + correction(env) + "{}";
			return string();
		}
		output += correction(env);
	} else
		output += env;
	output += "}";
	if (suffix == "\\begin")
		output += "\n";
	return output;
}


void adjust_row_column(string const & str, TexRow & texrow, int & column)
{
	if (!contains(str, "\n"))
		column += str.size();
	else {
		string tmp;
		texrow.newline();
		column = rsplit(str, tmp, '\n').size();
	}
}

} // namespace anon


int Paragraph::Private::startTeXParParams(BufferParams const & bparams,
				 odocstream & os, TexRow & texrow,
				 OutputParams const & runparams) const
{
	int column = 0;

	if (params_.noindent()) {
		os << "\\noindent ";
		column += 10;
	}
	
	LyXAlignment const curAlign = params_.align();

	if (curAlign == layout_->align)
		return column;

	switch (curAlign) {
	case LYX_ALIGN_NONE:
	case LYX_ALIGN_BLOCK:
	case LYX_ALIGN_LAYOUT:
	case LYX_ALIGN_SPECIAL:
	case LYX_ALIGN_DECIMAL:
		break;
	case LYX_ALIGN_LEFT:
	case LYX_ALIGN_RIGHT:
	case LYX_ALIGN_CENTER:
		if (runparams.moving_arg) {
			os << "\\protect";
			column += 8;
		}
		break;
	}

	string const begin_tag = "\\begin";
	InsetCode code = ownerCode();
	bool const lastpar = runparams.isLastPar;

	switch (curAlign) {
	case LYX_ALIGN_NONE:
	case LYX_ALIGN_BLOCK:
	case LYX_ALIGN_LAYOUT:
	case LYX_ALIGN_SPECIAL:
	case LYX_ALIGN_DECIMAL:
		break;
	case LYX_ALIGN_LEFT: {
		string output;
		if (owner_->getParLanguage(bparams)->babel() != "hebrew")
			output = corrected_env(begin_tag, "flushleft", code, lastpar);
		else
			output = corrected_env(begin_tag, "flushright", code, lastpar);
		os << from_ascii(output);
		adjust_row_column(output, texrow, column);
		break;
	} case LYX_ALIGN_RIGHT: {
		string output;
		if (owner_->getParLanguage(bparams)->babel() != "hebrew")
			output = corrected_env(begin_tag, "flushright", code, lastpar);
		else
			output = corrected_env(begin_tag, "flushleft", code, lastpar);
		os << from_ascii(output);
		adjust_row_column(output, texrow, column);
		break;
	} case LYX_ALIGN_CENTER: {
		string output;
		output = corrected_env(begin_tag, "center", code, lastpar);
		os << from_ascii(output);
		adjust_row_column(output, texrow, column);
		break;
	}
	}

	return column;
}


int Paragraph::Private::endTeXParParams(BufferParams const & bparams,
			       odocstream & os, TexRow & texrow,
			       OutputParams const & runparams) const
{
	int column = 0;

	LyXAlignment const curAlign = params_.align();

	if (curAlign == layout_->align)
		return column;

	switch (curAlign) {
	case LYX_ALIGN_NONE:
	case LYX_ALIGN_BLOCK:
	case LYX_ALIGN_LAYOUT:
	case LYX_ALIGN_SPECIAL:
	case LYX_ALIGN_DECIMAL:
		break;
	case LYX_ALIGN_LEFT:
	case LYX_ALIGN_RIGHT:
	case LYX_ALIGN_CENTER:
		if (runparams.moving_arg) {
			os << "\\protect";
			column = 8;
		}
		break;
	}

	string const end_tag = "\n\\par\\end";
	InsetCode code = ownerCode();
	bool const lastpar = runparams.isLastPar;

	switch (curAlign) {
	case LYX_ALIGN_NONE:
	case LYX_ALIGN_BLOCK:
	case LYX_ALIGN_LAYOUT:
	case LYX_ALIGN_SPECIAL:
	case LYX_ALIGN_DECIMAL:
		break;
	case LYX_ALIGN_LEFT: {
		string output;
		if (owner_->getParLanguage(bparams)->babel() != "hebrew")
			output = corrected_env(end_tag, "flushleft", code, lastpar);
		else
			output = corrected_env(end_tag, "flushright", code, lastpar);
		os << from_ascii(output);
		adjust_row_column(output, texrow, column);
		break;
	} case LYX_ALIGN_RIGHT: {
		string output;
		if (owner_->getParLanguage(bparams)->babel() != "hebrew")
			output = corrected_env(end_tag, "flushright", code, lastpar);
		else
			output = corrected_env(end_tag, "flushleft", code, lastpar);
		os << from_ascii(output);
		adjust_row_column(output, texrow, column);
		break;
	} case LYX_ALIGN_CENTER: {
		string output;
		output = corrected_env(end_tag, "center", code, lastpar);
		os << from_ascii(output);
		adjust_row_column(output, texrow, column);
		break;
	}
	}

	return column;
}


// This one spits out the text of the paragraph
void Paragraph::latex(BufferParams const & bparams,
	Font const & outerfont,
	odocstream & os, TexRow & texrow,
	OutputParams const & runparams,
	int start_pos, int end_pos, bool force) const
{
	LYXERR(Debug::LATEX, "Paragraph::latex...     " << this);

	// FIXME This check should not be needed. Perhaps issue an
	// error if it triggers.
	Layout const & style = inInset().forcePlainLayout() ?
		bparams.documentClass().plainLayout() : *d->layout_;

	if (!force && style.inpreamble)
		return;

	bool const allowcust = allowParagraphCustomization();

	// Current base font for all inherited font changes, without any
	// change caused by an individual character, except for the language:
	// It is set to the language of the first character.
	// As long as we are in the label, this font is the base font of the
	// label. Before the first body character it is set to the base font
	// of the body.
	Font basefont;

	// Maybe we have to create a optional argument.
	pos_type body_pos = beginOfBody();
	unsigned int column = 0;

	if (body_pos > 0) {
		// the optional argument is kept in curly brackets in
		// case it contains a ']'
		os << "[{";
		column += 2;
		basefont = getLabelFont(bparams, outerfont);
	} else {
		basefont = getLayoutFont(bparams, outerfont);
	}

	// Which font is currently active?
	Font running_font(basefont);
	// Do we have an open font change?
	bool open_font = false;

	Change runningChange = Change(Change::UNCHANGED);

	Encoding const * const prev_encoding = runparams.encoding;

	texrow.start(id(), 0);

	// if the paragraph is empty, the loop will not be entered at all
	if (empty()) {
		if (style.isCommand()) {
			os << '{';
			++column;
		}
		if (allowcust)
			column += d->startTeXParParams(bparams, os, texrow,
						    runparams);
	}

	for (pos_type i = 0; i < size(); ++i) {
		// First char in paragraph or after label?
		if (i == body_pos) {
			if (body_pos > 0) {
				if (open_font) {
					column += running_font.latexWriteEndChanges(
						os, bparams, runparams,
						basefont, basefont);
					open_font = false;
				}
				basefont = getLayoutFont(bparams, outerfont);
				running_font = basefont;

				column += Changes::latexMarkChange(os, bparams,
						runningChange, Change(Change::UNCHANGED),
						runparams);
				runningChange = Change(Change::UNCHANGED);

				os << "}] ";
				column +=3;
			}
			if (style.isCommand()) {
				os << '{';
				++column;
			}

			if (allowcust)
				column += d->startTeXParParams(bparams, os,
							    texrow,
							    runparams);
		}

		Change const & change = runparams.inDeletedInset ? runparams.changeOfDeletedInset
		                                                 : lookupChange(i);

		if (bparams.outputChanges && runningChange != change) {
			if (open_font) {
				column += running_font.latexWriteEndChanges(
						os, bparams, runparams, basefont, basefont);
				open_font = false;
			}
			basefont = getLayoutFont(bparams, outerfont);
			running_font = basefont;

			column += Changes::latexMarkChange(os, bparams, runningChange,
							   change, runparams);
			runningChange = change;
		}

		// do not output text which is marked deleted
		// if change tracking output is disabled
		if (!bparams.outputChanges && change.deleted()) {
			continue;
		}

		++column;

		// Fully instantiated font
		Font const font = getFont(bparams, i, outerfont);

		Font const last_font = running_font;

		// Do we need to close the previous font?
		if (open_font &&
		    (font != running_font ||
		     font.language() != running_font.language()))
		{
			column += running_font.latexWriteEndChanges(
					os, bparams, runparams, basefont,
					(i == body_pos-1) ? basefont : font);
			running_font = basefont;
			open_font = false;
		}

		// close babel's font environment before opening CJK.
		if (!running_font.language()->babel().empty() &&
		    font.language()->encoding()->package() == Encoding::CJK) {
				string end_tag = subst(lyxrc.language_command_end,
							"$$lang",
							running_font.language()->babel());
				os << from_ascii(end_tag);
				column += end_tag.length();
		}

		// Switch file encoding if necessary (and allowed)
		if (!runparams.pass_thru && !style.pass_thru &&
		    runparams.encoding->package() != Encoding::none &&
		    font.language()->encoding()->package() != Encoding::none) {
			pair<bool, int> const enc_switch = switchEncoding(os, bparams,
					runparams, *(font.language()->encoding()));
			if (enc_switch.first) {
				column += enc_switch.second;
				runparams.encoding = font.language()->encoding();
			}
		}

		char_type const c = d->text_[i];

		// Do we need to change font?
		if ((font != running_font ||
		     font.language() != running_font.language()) &&
			i != body_pos - 1)
		{
			odocstringstream ods;
			column += font.latexWriteStartChanges(ods, bparams,
							      runparams, basefont,
							      last_font);
			running_font = font;
			open_font = true;
			docstring fontchange = ods.str();
			// check whether the fontchange ends with a \\textcolor
			// modifier and the text starts with a space (bug 4473)
			docstring const last_modifier = rsplit(fontchange, '\\');
			if (prefixIs(last_modifier, from_ascii("textcolor")) && c == ' ')
				os << fontchange << from_ascii("{}");
			// check if the fontchange ends with a trailing blank
			// (like "\small " (see bug 3382)
			else if (suffixIs(fontchange, ' ') && c == ' ')
				os << fontchange.substr(0, fontchange.size() - 1) 
				   << from_ascii("{}");
			else
				os << fontchange;
		}

		// FIXME: think about end_pos implementation...
		if (c == ' ' && i >= start_pos && (end_pos == -1 || i < end_pos)) {
			// FIXME: integrate this case in latexSpecialChar
			// Do not print the separation of the optional argument
			// if style.pass_thru is false. This works because
			// latexSpecialChar ignores spaces if
			// style.pass_thru is false.
			if (i != body_pos - 1) {
				if (d->simpleTeXBlanks(
						runparams, os, texrow,
						i, column, font, style)) {
					// A surrogate pair was output. We
					// must not call latexSpecialChar
					// in this iteration, since it would output
					// the combining character again.
					++i;
					continue;
				}
			}
		}

		OutputParams rp = runparams;
		rp.free_spacing = style.free_spacing;
		rp.local_font = &font;
		rp.intitle = style.intitle;

		// Two major modes:  LaTeX or plain
		// Handle here those cases common to both modes
		// and then split to handle the two modes separately.
		if (c == META_INSET) {
			if (i >= start_pos && (end_pos == -1 || i < end_pos)) {
				d->latexInset(bparams, os,
						texrow, rp, running_font,
						basefont, outerfont, open_font,
						runningChange, style, i, column);
			}
		} else {
			if (i >= start_pos && (end_pos == -1 || i < end_pos)) {
				try {
					d->latexSpecialChar(os, rp, running_font, runningChange,
						style, i, column);
				} catch (EncodingException & e) {
				if (runparams.dryrun) {
					os << "<" << _("LyX Warning: ")
					   << _("uncodable character") << " '";
					os.put(c);
					os << "'>";
				} else {
					// add location information and throw again.
					e.par_id = id();
					e.pos = i;
					throw(e);
				}
			}
		}
		}

		// Set the encoding to that returned from latexSpecialChar (see
		// comment for encoding member in OutputParams.h)
		runparams.encoding = rp.encoding;
	}

	// If we have an open font definition, we have to close it
	if (open_font) {
#ifdef FIXED_LANGUAGE_END_DETECTION
		if (next_) {
			running_font.latexWriteEndChanges(os, bparams,
					runparams, basefont,
					next_->getFont(bparams, 0, outerfont));
		} else {
			running_font.latexWriteEndChanges(os, bparams,
					runparams, basefont, basefont);
		}
#else
//FIXME: For now we ALWAYS have to close the foreign font settings if they are
//FIXME: there as we start another \selectlanguage with the next paragraph if
//FIXME: we are in need of this. This should be fixed sometime (Jug)
		running_font.latexWriteEndChanges(os, bparams, runparams,
				basefont, basefont);
#endif
	}

	column += Changes::latexMarkChange(os, bparams, runningChange,
					   Change(Change::UNCHANGED), runparams);

	// Needed if there is an optional argument but no contents.
	if (body_pos > 0 && body_pos == size()) {
		os << "}]~";
	}

	if (allowcust && d->endTeXParParams(bparams, os, texrow, runparams)
	    && runparams.encoding != prev_encoding) {
		runparams.encoding = prev_encoding;
		if (!bparams.useXetex)
			os << setEncoding(prev_encoding->iconvName());
	}

	LYXERR(Debug::LATEX, "Paragraph::latex... done " << this);
}


bool Paragraph::emptyTag() const
{
	for (pos_type i = 0; i < size(); ++i) {
		if (Inset const * inset = getInset(i)) {
			InsetCode lyx_code = inset->lyxCode();
			// FIXME testing like that is wrong. What is
			// the intent?
			if (lyx_code != TOC_CODE &&
			    lyx_code != INCLUDE_CODE &&
			    lyx_code != GRAPHICS_CODE &&
			    lyx_code != ERT_CODE &&
			    lyx_code != LISTINGS_CODE &&
			    lyx_code != FLOAT_CODE &&
			    lyx_code != TABULAR_CODE) {
				return false;
			}
		} else {
			char_type c = d->text_[i];
			if (c != ' ' && c != '\t')
				return false;
		}
	}
	return true;
}


string Paragraph::getID(Buffer const & buf, OutputParams const & runparams)
	const
{
	for (pos_type i = 0; i < size(); ++i) {
		if (Inset const * inset = getInset(i)) {
			InsetCode lyx_code = inset->lyxCode();
			if (lyx_code == LABEL_CODE) {
				InsetLabel const * const il = static_cast<InsetLabel const *>(inset);
				docstring const & id = il->getParam("name");
				return "id='" + to_utf8(sgml::cleanID(buf, runparams, id)) + "'";
			}
		}
	}
	return string();
}


pos_type Paragraph::firstWordDocBook(odocstream & os, OutputParams const & runparams)
	const
{
	pos_type i;
	for (i = 0; i < size(); ++i) {
		if (Inset const * inset = getInset(i)) {
			inset->docbook(os, runparams);
		} else {
			char_type c = d->text_[i];
			if (c == ' ')
				break;
			os << sgml::escapeChar(c);
		}
	}
	return i;
}


pos_type Paragraph::firstWordLyXHTML(XHTMLStream & xs, OutputParams const & runparams)
	const
{
	pos_type i;
	for (i = 0; i < size(); ++i) {
		if (Inset const * inset = getInset(i)) {
			inset->xhtml(xs, runparams);
		} else {
			char_type c = d->text_[i];
			if (c == ' ')
				break;
			xs << c;
		}
	}
	return i;
}


bool Paragraph::Private::onlyText(Buffer const & buf, Font const & outerfont, pos_type initial) const
{
	Font font_old;
	pos_type size = text_.size();
	for (pos_type i = initial; i < size; ++i) {
		Font font = owner_->getFont(buf.params(), i, outerfont);
		if (text_[i] == META_INSET)
			return false;
		if (i != initial && font != font_old)
			return false;
		font_old = font;
	}

	return true;
}


void Paragraph::simpleDocBookOnePar(Buffer const & buf,
				    odocstream & os,
				    OutputParams const & runparams,
				    Font const & outerfont,
				    pos_type initial) const
{
	bool emph_flag = false;

	Layout const & style = *d->layout_;
	FontInfo font_old =
		style.labeltype == LABEL_MANUAL ? style.labelfont : style.font;

	if (style.pass_thru && !d->onlyText(buf, outerfont, initial))
		os << "]]>";

	// parsing main loop
	for (pos_type i = initial; i < size(); ++i) {
		Font font = getFont(buf.params(), i, outerfont);

		// handle <emphasis> tag
		if (font_old.emph() != font.fontInfo().emph()) {
			if (font.fontInfo().emph() == FONT_ON) {
				os << "<emphasis>";
				emph_flag = true;
			} else if (i != initial) {
				os << "</emphasis>";
				emph_flag = false;
			}
		}

		if (Inset const * inset = getInset(i)) {
			inset->docbook(os, runparams);
		} else {
			char_type c = d->text_[i];

			if (style.pass_thru)
				os.put(c);
			else
				os << sgml::escapeChar(c);
		}
		font_old = font.fontInfo();
	}

	if (emph_flag) {
		os << "</emphasis>";
	}

	if (style.free_spacing)
		os << '\n';
	if (style.pass_thru && !d->onlyText(buf, outerfont, initial))
		os << "<![CDATA[";
}


docstring Paragraph::simpleLyXHTMLOnePar(Buffer const & buf,
				    XHTMLStream & xs,
				    OutputParams const & runparams,
				    Font const & outerfont,
				    pos_type initial) const
{
	docstring retval;

	bool emph_flag = false;
	bool bold_flag = false;
	string closing_tag;

	Layout const & style = *d->layout_;

	if (!runparams.for_toc && runparams.html_make_pars) {
		// generate a magic label for this paragraph
		string const attr = "id='" + magicLabel() + "'";
		xs << html::CompTag("a", attr);
	}

	FontInfo font_old =
		style.labeltype == LABEL_MANUAL ? style.labelfont : style.font;

	// parsing main loop
	for (pos_type i = initial; i < size(); ++i) {
		// let's not show deleted material in the output
		if (isDeleted(i))
			continue;
	
		Font font = getFont(buf.params(), i, outerfont);

		// emphasis
		if (font_old.emph() != font.fontInfo().emph()) {
			if (font.fontInfo().emph() == FONT_ON) {
				xs << html::StartTag("em");
				emph_flag = true;
			} else if (emph_flag && i != initial) {
				xs << html::EndTag("em");
				emph_flag = false;
			}
		}
		// bold
		if (font_old.series() != font.fontInfo().series()) {
			if (font.fontInfo().series() == BOLD_SERIES) {
				xs << html::StartTag("strong");
				bold_flag = true;
			} else if (bold_flag && i != initial) {
				xs << html::EndTag("strong");
				bold_flag = false;
			}
		}
		// FIXME XHTML
		// Other such tags? What about the other text ranges?

		Inset const * inset = getInset(i);
		if (inset) {
			InsetCommand const * ic = inset->asInsetCommand();
			InsetLayout const & il = inset->getLayout();
			InsetMath const * im = inset->asInsetMath();
			if (!runparams.for_toc 
			    || im || il.isInToc() || (ic && ic->isInToc())) {
				OutputParams np = runparams;
				if (!il.htmlisblock())
					np.html_in_par = true;
				retval += inset->xhtml(xs, np);
			}
		} else {
			char_type c = d->text_[i];

			if (style.pass_thru)
				xs << c;
			else if (c == '-') {
				docstring str;
				int j = i + 1;
				if (j < size() && d->text_[j] == '-') {
					j += 1;
					if (j < size() && d->text_[j] == '-') {
						str += from_ascii("&mdash;");
						i += 2;
					} else {
						str += from_ascii("&ndash;");
						i += 1;
					}
				}
				else
					str += c;
				// We don't want to escape the entities. Note that
				// it is safe to do this, since str can otherwise
				// only be "-". E.g., it can't be "<".
				xs << XHTMLStream::NextRaw() << str;
			} else
				xs << c;
		}
		font_old = font.fontInfo();
	}

	xs.closeFontTags();
	return retval;
}


bool Paragraph::isHfill(pos_type pos) const
{
	Inset const * inset = getInset(pos);
	return inset && (inset->lyxCode() == SPACE_CODE &&
			 inset->isStretchableSpace());
}


bool Paragraph::isNewline(pos_type pos) const
{
	Inset const * inset = getInset(pos);
	return inset && inset->lyxCode() == NEWLINE_CODE;
}


bool Paragraph::isLineSeparator(pos_type pos) const
{
	char_type const c = d->text_[pos];
	if (isLineSeparatorChar(c))
		return true;
	Inset const * inset = getInset(pos);
	return inset && inset->isLineSeparator();
}


bool Paragraph::isWordSeparator(pos_type pos) const
{
	if (Inset const * inset = getInset(pos))
		return !inset->isLetter();
	char_type const c = d->text_[pos];
	// We want to pass the ' and escape chars to the spellchecker
	static docstring const quote = from_utf8(lyxrc.spellchecker_esc_chars + '\'');
	return (!isLetterChar(c) && !isDigit(c) && !contains(quote, c))
		|| pos == size();
}


bool Paragraph::isChar(pos_type pos) const
{
	if (Inset const * inset = getInset(pos))
		return inset->isChar();
	char_type const c = d->text_[pos];
	return !isLetterChar(c) && !isDigit(c) && !lyx::isSpace(c);
}


bool Paragraph::isSpace(pos_type pos) const
{
	if (Inset const * inset = getInset(pos))
		return inset->isSpace();
	char_type const c = d->text_[pos];
	return lyx::isSpace(c);
}


Language const *
Paragraph::getParLanguage(BufferParams const & bparams) const
{
	if (!empty())
		return getFirstFontSettings(bparams).language();
	// FIXME: we should check the prev par as well (Lgb)
	return bparams.language;
}


bool Paragraph::isRTL(BufferParams const & bparams) const
{
	return lyxrc.rtl_support
		&& getParLanguage(bparams)->rightToLeft()
		&& !inInset().getLayout().forceLTR();
}


void Paragraph::changeLanguage(BufferParams const & bparams,
			       Language const * from, Language const * to)
{
	// change language including dummy font change at the end
	for (pos_type i = 0; i <= size(); ++i) {
		Font font = getFontSettings(bparams, i);
		if (font.language() == from) {
			font.setLanguage(to);
			setFont(i, font);
		}
	}
}


bool Paragraph::isMultiLingual(BufferParams const & bparams) const
{
	Language const * doc_language = bparams.language;
	FontList::const_iterator cit = d->fontlist_.begin();
	FontList::const_iterator end = d->fontlist_.end();

	for (; cit != end; ++cit)
		if (cit->font().language() != ignore_language &&
		    cit->font().language() != latex_language &&
		    cit->font().language() != doc_language)
			return true;
	return false;
}


void Paragraph::getLanguages(std::set<Language const *> & languages) const
{
	FontList::const_iterator cit = d->fontlist_.begin();
	FontList::const_iterator end = d->fontlist_.end();

	for (; cit != end; ++cit) {
		Language const * lang = cit->font().language();
		if (lang != ignore_language &&
		    lang != latex_language)
			languages.insert(lang);
	}
}


docstring Paragraph::asString(int options) const
{
	return asString(0, size(), options);
}


docstring Paragraph::asString(pos_type beg, pos_type end, int options) const
{
	odocstringstream os;

	if (beg == 0 
	    && options & AS_STR_LABEL
	    && !d->params_.labelString().empty())
		os << d->params_.labelString() << ' ';

	for (pos_type i = beg; i < end; ++i) {
		char_type const c = d->text_[i];
		if (isPrintable(c) || c == '\t'
		    || (c == '\n' && (options & AS_STR_NEWLINES)))
			os.put(c);
		else if (c == META_INSET && (options & AS_STR_INSETS)) {
			getInset(i)->tocString(os);
			if (getInset(i)->asInsetMath())
				os << " ";
		}
	}

	return os.str();
}


docstring Paragraph::stringify(pos_type beg, pos_type end, int options, OutputParams & runparams) const
{
	odocstringstream os;

	if (beg == 0 
		&& options & AS_STR_LABEL
		&& !d->params_.labelString().empty())
		os << d->params_.labelString() << ' ';

	for (pos_type i = beg; i < end; ++i) {
		char_type const c = d->text_[i];
		if (isPrintable(c) || c == '\t'
		    || (c == '\n' && (options & AS_STR_NEWLINES)))
			os.put(c);
		else if (c == META_INSET && (options & AS_STR_INSETS)) {
			getInset(i)->plaintext(os, runparams);
		}
	}

	return os.str();
}


void Paragraph::setInsetOwner(Inset const * inset)
{
	d->inset_owner_ = inset;
}


int Paragraph::id() const
{
	return d->id_;
}


void Paragraph::setId(int id)
{
	d->id_ = id;
}


Layout const & Paragraph::layout() const
{
	return *d->layout_;
}


void Paragraph::setLayout(Layout const & layout)
{
	d->layout_ = &layout;
}


void Paragraph::setDefaultLayout(DocumentClass const & tc)
{ 
	setLayout(tc.defaultLayout()); 
}


void Paragraph::setPlainLayout(DocumentClass const & tc)
{ 
	setLayout(tc.plainLayout()); 
}


void Paragraph::setPlainOrDefaultLayout(DocumentClass const & tclass)
{
	if (usePlainLayout())
		setPlainLayout(tclass);
	else
		setDefaultLayout(tclass);
}


Inset const & Paragraph::inInset() const
{
	LASSERT(d->inset_owner_, throw ExceptionMessage(BufferException,
		_("Memory problem"), _("Paragraph not properly initialized")));
	return *d->inset_owner_;
}


ParagraphParameters & Paragraph::params()
{
	return d->params_;
}


ParagraphParameters const & Paragraph::params() const
{
	return d->params_;
}


bool Paragraph::isFreeSpacing() const
{
	if (d->layout_->free_spacing)
		return true;
	return d->inset_owner_ && d->inset_owner_->isFreeSpacing();
}


bool Paragraph::allowEmpty() const
{
	if (d->layout_->keepempty)
		return true;
	return d->inset_owner_ && d->inset_owner_->allowEmpty();
}


char_type Paragraph::transformChar(char_type c, pos_type pos) const
{
	if (!Encodings::isArabicChar(c))
		return c;

	char_type prev_char = ' ';
	char_type next_char = ' ';

	for (pos_type i = pos - 1; i >= 0; --i) {
		char_type const par_char = d->text_[i];
		if (!Encodings::isArabicComposeChar(par_char)) {
			prev_char = par_char;
			break;
		}
	}

	for (pos_type i = pos + 1, end = size(); i < end; ++i) {
		char_type const par_char = d->text_[i];
		if (!Encodings::isArabicComposeChar(par_char)) {
			next_char = par_char;
			break;
		}
	}

	if (Encodings::isArabicChar(next_char)) {
		if (Encodings::isArabicChar(prev_char) &&
			!Encodings::isArabicSpecialChar(prev_char))
			return Encodings::transformChar(c, Encodings::FORM_MEDIAL);
		else
			return Encodings::transformChar(c, Encodings::FORM_INITIAL);
	} else {
		if (Encodings::isArabicChar(prev_char) &&
			!Encodings::isArabicSpecialChar(prev_char))
			return Encodings::transformChar(c, Encodings::FORM_FINAL);
		else
			return Encodings::transformChar(c, Encodings::FORM_ISOLATED);
	}
}


int Paragraph::checkBiblio(Buffer const & buffer)
{
	// FIXME From JS:
	// This is getting more and more a mess. ...We really should clean
	// up this bibitem issue for 1.6. See also bug 2743.

	// Add bibitem insets if necessary
	if (d->layout_->labeltype != LABEL_BIBLIO)
		return 0;

	bool hasbibitem = !d->insetlist_.empty()
		// Insist on it being in pos 0
		&& d->text_[0] == META_INSET
		&& d->insetlist_.begin()->inset->lyxCode() == BIBITEM_CODE;

	bool track_changes = buffer.params().trackChanges;

	docstring oldkey;
	docstring oldlabel;

	// remove a bibitem in pos != 0
	// restore it later in pos 0 if necessary
	// (e.g. if a user inserts contents _before_ the item)
	// we're assuming there's only one of these, which there
	// should be.
	int erasedInsetPosition = -1;
	InsetList::iterator it = d->insetlist_.begin();
	InsetList::iterator end = d->insetlist_.end();
	for (; it != end; ++it)
		if (it->inset->lyxCode() == BIBITEM_CODE
		    && it->pos > 0) {
			InsetBibitem * olditem = static_cast<InsetBibitem *>(it->inset);
			oldkey = olditem->getParam("key");
			oldlabel = olditem->getParam("label");
			erasedInsetPosition = it->pos;
			eraseChar(erasedInsetPosition, track_changes);
			break;
	}

	// There was an InsetBibitem at the beginning, and we didn't
	// have to erase one.
	if (hasbibitem && erasedInsetPosition < 0)
			return 0;

	// There was an InsetBibitem at the beginning and we did have to
	// erase one. So we give its properties to the beginning inset.
	if (hasbibitem) {
		InsetBibitem * inset =
			static_cast<InsetBibitem *>(d->insetlist_.begin()->inset);
		if (!oldkey.empty())
			inset->setParam("key", oldkey);
		inset->setParam("label", oldlabel);
		return -erasedInsetPosition;
	}

	// There was no inset at the beginning, so we need to create one with
	// the key and label of the one we erased.
	InsetBibitem * inset = 
		new InsetBibitem(const_cast<Buffer *>(&buffer), InsetCommandParams(BIBITEM_CODE));
	// restore values of previously deleted item in this par.
	if (!oldkey.empty())
		inset->setParam("key", oldkey);
	inset->setParam("label", oldlabel);
	insertInset(0, static_cast<Inset *>(inset),
		    Change(track_changes ? Change::INSERTED : Change::UNCHANGED));

	return 1;
}


void Paragraph::checkAuthors(AuthorList const & authorList)
{
	d->changes_.checkAuthors(authorList);
}


bool Paragraph::isChanged(pos_type pos) const
{
	return lookupChange(pos).changed();
}


bool Paragraph::isInserted(pos_type pos) const
{
	return lookupChange(pos).inserted();
}


bool Paragraph::isDeleted(pos_type pos) const
{
	return lookupChange(pos).deleted();
}


InsetList const & Paragraph::insetList() const
{
	return d->insetlist_;
}


void Paragraph::setBuffer(Buffer & b)
{
	d->insetlist_.setBuffer(b);
}


Inset * Paragraph::releaseInset(pos_type pos)
{
	Inset * inset = d->insetlist_.release(pos);
	/// does not honour change tracking!
	eraseChar(pos, false);
	return inset;
}


Inset * Paragraph::getInset(pos_type pos)
{
	return (pos < pos_type(d->text_.size()) && d->text_[pos] == META_INSET)
		 ? d->insetlist_.get(pos) : 0;
}


Inset const * Paragraph::getInset(pos_type pos) const
{
	return (pos < pos_type(d->text_.size()) && d->text_[pos] == META_INSET)
		 ? d->insetlist_.get(pos) : 0;
}


void Paragraph::changeCase(BufferParams const & bparams, pos_type pos,
		pos_type & right, TextCase action)
{
	// process sequences of modified characters; in change
	// tracking mode, this approach results in much better
	// usability than changing case on a char-by-char basis
	docstring changes;

	bool const trackChanges = bparams.trackChanges;

	bool capitalize = true;

	for (; pos < right; ++pos) {
		char_type oldChar = d->text_[pos];
		char_type newChar = oldChar;

		// ignore insets and don't play with deleted text!
		if (oldChar != META_INSET && !isDeleted(pos)) {
			switch (action) {
				case text_lowercase:
					newChar = lowercase(oldChar);
					break;
				case text_capitalization:
					if (capitalize) {
						newChar = uppercase(oldChar);
						capitalize = false;
					}
					break;
				case text_uppercase:
					newChar = uppercase(oldChar);
					break;
			}
		}

		if (isWordSeparator(pos) || isDeleted(pos)) {
			// permit capitalization again
			capitalize = true;
		}

		if (oldChar != newChar) {
			changes += newChar;
			if (pos != right - 1)
				continue;
			// step behind the changing area
			pos++;
		}

		int erasePos = pos - changes.size();
		for (size_t i = 0; i < changes.size(); i++) {
			insertChar(pos, changes[i],
				   getFontSettings(bparams,
						   erasePos),
				   trackChanges);
			if (!eraseChar(erasePos, trackChanges)) {
				++erasePos;
				++pos; // advance
				++right; // expand selection
			}
		}
		changes.clear();
	}
}


bool Paragraph::find(docstring const & str, bool cs, bool mw,
		pos_type pos, bool del) const
{
	int const strsize = str.length();
	int i = 0;
	pos_type const parsize = d->text_.size();
	for (i = 0; pos + i < parsize; ++i) {
		if (i >= strsize)
			break;
		if (cs && str[i] != d->text_[pos + i])
			break;
		if (!cs && uppercase(str[i]) != uppercase(d->text_[pos + i]))
			break;
		if (!del && isDeleted(pos + i))
			break;
	}

	if (i != strsize)
		return false;

	// if necessary, check whether string matches word
	if (mw) {
		if (pos > 0 && !isWordSeparator(pos - 1))
			return false;
		if (pos + strsize < parsize
			&& !isWordSeparator(pos + strsize))
			return false;
	}

	return true;
}


char_type Paragraph::getChar(pos_type pos) const
{
	return d->text_[pos];
}


pos_type Paragraph::size() const
{
	return d->text_.size();
}


bool Paragraph::empty() const
{
	return d->text_.empty();
}


bool Paragraph::isInset(pos_type pos) const
{
	return d->text_[pos] == META_INSET;
}


bool Paragraph::isSeparator(pos_type pos) const
{
	//FIXME: Are we sure this can be the only separator?
	return d->text_[pos] == ' ';
}


void Paragraph::deregisterWords()
{
	Private::LangWordsMap::const_iterator itl = d->words_.begin();
	Private::LangWordsMap::const_iterator ite = d->words_.end();
	for (; itl != ite; ++itl) {
		WordList * wl = theWordList(itl->first);
		Private::Words::const_iterator it = (itl->second).begin();
		Private::Words::const_iterator et = (itl->second).end();
		for (; it != et; ++it)
			wl->remove(*it);
	}
	d->words_.clear();
}


void Paragraph::locateWord(pos_type & from, pos_type & to,
	word_location const loc) const
{
	switch (loc) {
	case WHOLE_WORD_STRICT:
		if (from == 0 || from == size()
		    || isWordSeparator(from)
		    || isWordSeparator(from - 1)) {
			to = from;
			return;
		}
		// no break here, we go to the next

	case WHOLE_WORD:
		// If we are already at the beginning of a word, do nothing
		if (!from || isWordSeparator(from - 1))
			break;
		// no break here, we go to the next

	case PREVIOUS_WORD:
		// always move the cursor to the beginning of previous word
		while (from && !isWordSeparator(from - 1))
			--from;
		break;
	case NEXT_WORD:
		LYXERR0("Paragraph::locateWord: NEXT_WORD not implemented yet");
		break;
	case PARTIAL_WORD:
		// no need to move the 'from' cursor
		break;
	}
	to = from;
	while (to < size() && !isWordSeparator(to))
		++to;
}


void Paragraph::collectWords()
{
	// This is the value that needs to be exposed in the preferences
	// to resolve bug #6760.
	static int minlength = 6;
	pos_type n = size();
	for (pos_type pos = 0; pos < n; ++pos) {
		if (isWordSeparator(pos))
			continue;
		pos_type from = pos;
		locateWord(from, pos, WHOLE_WORD);
		if (pos - from >= minlength) {
			docstring word = asString(from, pos, AS_STR_NONE);
			FontList::const_iterator cit = d->fontlist_.fontIterator(pos);
			if (cit == d->fontlist_.end())
				return;
			Language const * lang = cit->font().language();
			d->words_[*lang].insert(word);
		}
	}
}


void Paragraph::registerWords()
{
	Private::LangWordsMap::const_iterator itl = d->words_.begin();
	Private::LangWordsMap::const_iterator ite = d->words_.end();
	for (; itl != ite; ++itl) {
		WordList * wl = theWordList(itl->first);
		Private::Words::const_iterator it = (itl->second).begin();
		Private::Words::const_iterator et = (itl->second).end();
		for (; it != et; ++it)
			wl->insert(*it);
	}
}


void Paragraph::updateWords()
{
	deregisterWords();
	collectWords();
	registerWords();
}


SpellChecker::Result Paragraph::spellCheck(pos_type & from, pos_type & to, WordLangTuple & wl,
	docstring_list & suggestions, bool do_suggestion) const
{
	SpellChecker * speller = theSpellChecker();
	if (!speller)
		return SpellChecker::WORD_OK;

	if (!d->layout_->spellcheck || !inInset().allowSpellCheck())
		return SpellChecker::WORD_OK;

	locateWord(from, to, WHOLE_WORD);
	if (from == to || from >= pos_type(d->text_.size()))
		return SpellChecker::WORD_OK;

	docstring word = asString(from, to, AS_STR_INSETS);
	// Ignore words with digits
	// FIXME: make this customizable
	// (note that hunspell ignores words with digits by default)
	bool const ignored = hasDigit(word);
	Language * lang = const_cast<Language *>(getFontSettings(
		    d->inset_owner_->buffer().params(), from).language());
	if (lang == d->inset_owner_->buffer().params().language
	    && !lyxrc.spellchecker_alt_lang.empty()) {
		string lang_code;
		string const lang_variety =
			split(lyxrc.spellchecker_alt_lang, lang_code, '-');
		lang->setCode(lang_code);
		lang->setVariety(lang_variety);
	}
	wl = WordLangTuple(word, lang);
	SpellChecker::Result res = ignored ?
		SpellChecker::WORD_OK : speller->check(wl);

	bool const misspelled_ = SpellChecker::misspelled(res) ;

	if (lyxrc.spellcheck_continuously)
		d->setMisspelled(from, to, misspelled_);

	if (misspelled_ && do_suggestion)
		speller->suggest(wl, suggestions);
	else
		suggestions.clear();

	return res;
}


bool Paragraph::isMisspelled(pos_type pos) const
{
	pos_type from = pos;
	pos_type to = pos;
	WordLangTuple wl;
	docstring_list suggestions;
	SpellChecker::Result res = spellCheck(from, to, wl, suggestions, false);
	return SpellChecker::misspelled(res) ;
}


string Paragraph::magicLabel() const
{
	stringstream ss;
	ss << "magicparlabel-" << id();
	return ss.str();
}


} // namespace lyx
