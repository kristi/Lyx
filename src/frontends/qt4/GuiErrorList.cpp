/**
 * \file GuiErrorList.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Alfredo Braunstein
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "GuiErrorList.h"

#include "GuiView.h"
#include "qt_helpers.h"

#include "Buffer.h"
#include "BufferView.h"
#include "FuncRequest.h"
#include "BufferList.h"
#include "ParIterator.h"
#include "Text.h"

#include "support/debug.h"
#include "support/gettext.h"
#include "support/lstrings.h"

#include <QListWidget>
#include <QPushButton>
#include <QShowEvent>
#include <QTextBrowser>

using namespace std;
using namespace lyx::support;

namespace {

string const guiErrorType(string const s)
{
	if (s == "docbook")
		return N_("DocBook");
	else if (s == "literate")
		return N_("Literate");
	else if (s == "platex")
		return N_("pLaTeX");
	else if (s == "latex")
		return N_("LaTeX");
	return s;
}

} // namespace anon

namespace lyx {
namespace frontend {

GuiErrorList::GuiErrorList(GuiView & lv)
	: GuiDialog(lv, "errorlist", qt_("Error List"))
{
	setupUi(this);

	connect(closePB, SIGNAL(clicked()),
		this, SLOT(slotClose()));
	connect(viewLogPB, SIGNAL(clicked()),
		this, SLOT(viewLog()));
	connect(errorsLW, SIGNAL(currentRowChanged(int)),
		this, SLOT(select()));

	bc().setPolicy(ButtonPolicy::OkCancelPolicy);
	bc().setCancel(closePB);
}


void GuiErrorList::showEvent(QShowEvent * e)
{
	select();
	paramsToDialog();
	e->accept();
}


void GuiErrorList::select()
{
	int const item = errorsLW->row(errorsLW->currentItem());
	if (item == -1)
		return;
	goTo(item);
	descriptionTB->setPlainText(
		toqstr(errorList()[item].description));
}


void GuiErrorList::viewLog()
{
	if (&buffer() != buf_) {
		if (!theBufferList().isLoaded(buf_))
			return;
		FuncRequest fr(LFUN_BUFFER_SWITCH, buf_->absFileName());
		dispatch(fr);
	}
	dispatch(FuncRequest(LFUN_DIALOG_SHOW, "latexlog"));
}


void GuiErrorList::paramsToDialog()
{
	setTitle(toqstr(name_));
	errorsLW->clear();
	descriptionTB->setPlainText(QString());

	ErrorList const & el = errorList();
	ErrorList::const_iterator it = el.begin();
	ErrorList::const_iterator const en = el.end();
	for (; it != en; ++it)
		errorsLW->addItem(toqstr(it->error));
	errorsLW->setCurrentRow(0);
}


ErrorList const & GuiErrorList::errorList() const
{
	if (&bufferview()->buffer() == buf_) {
		error_list_ = from_master_ ?
			bufferview()->buffer().masterBuffer()->errorList(error_type_)
			: bufferview()->buffer().errorList(error_type_);
	}
	return error_list_;
}


bool GuiErrorList::initialiseParams(string const & data)
{
	from_master_ = prefixIs(data, "from_master|");
	string error_type = data;
	if (from_master_)
		error_type = split(data, '|');
	error_type_ = error_type;
	buf_ = from_master_ ?
		bufferview()->buffer().masterBuffer()
		: &bufferview()->buffer();
	name_ = bformat(_("%1$s Errors (%2$s)"), 
			        _(guiErrorType(error_type)),
				    from_utf8(buf_->absFileName()));
	paramsToDialog();
	return true;
}


bool GuiErrorList::goTo(int item)
{
	if (&buffer() != buf_) {
		if (!theBufferList().isLoaded(buf_))
			return false;
		FuncRequest fr(LFUN_BUFFER_SWITCH, buf_->absFileName());
		dispatch(fr);
	}
	ErrorItem const & err = errorList()[item];

	if (err.par_id == -1)
		return false;

	if (from_master_)
		// FIXME: implement
		return false;

	DocIterator dit = buf_->getParFromID(err.par_id);

	if (dit == doc_iterator_end(buf_)) {
        // FIXME: Happens when loading a read-only doc with 
        // unknown layout. Should this be the case?
		LYXERR0("par id " << err.par_id << " not found");
		return false;
	}

	// Now make the selection.
	// if pos_end is 0, this means it is end-of-paragraph
	pos_type const s = dit.paragraph().size();
	pos_type const end = err.pos_end ? min(err.pos_end, s) : s;
	pos_type const start = min(err.pos_start, end);
	pos_type const range = end - start;
	dit.pos() = start;
	BufferView * bv = const_cast<BufferView *>(bufferview());
	// FIXME: If we used an LFUN, we would not need this line:
	bv->putSelectionAt(dit, range, false);
	bv->processUpdateFlags(Update::Force | Update::FitCursor);
	return true;
}


Dialog * createGuiErrorList(GuiView & lv) { return new GuiErrorList(lv); }

} // namespace frontend
} // namespace lyx


#include "moc_GuiErrorList.cpp"
