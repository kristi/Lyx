// -*- C++ -*-
/**
 * \file GuiTabular.h
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author John Levon
 * \author Kalle Dalheimer
 * \author Jürgen Spitzmüller
 * \author Herbert Voß
 *
 * Full author contact details are available in file CREDITS.
 */

#ifndef GUITABULAR_H
#define GUITABULAR_H

#include "InsetParamsWidget.h"
#include "ui_TabularUi.h"
#include "insets/InsetTabular.h"

namespace lyx {
namespace frontend {

class GuiTabular : public InsetParamsWidget, public Ui::TabularUi
{
	Q_OBJECT

public:
	GuiTabular(QWidget * parent = 0);

private Q_SLOTS:
	void checkEnabled();
	void borderSet_clicked();
	void borderUnset_clicked();

private:
	/// \name InsetParamsWidget inherited methods
	//@{
	InsetCode insetCode() const { return TABULAR_CODE; }
	FuncCode creationCode() const { return LFUN_TABULAR_INSERT; }
	void paramsToDialog(Inset const *);
	docstring dialogToParams() const;
	//@}

	///
	void setHAlign(std::string & param_str) const;
	///
	void setVAlign(std::string & param_str) const;
	///
	void setTableAlignment(std::string & param_str) const;
	///
	void setWidthAndAlignment();
	///
	bool funcEnabled(Tabular::Feature f) const;
};

} // namespace frontend
} // namespace lyx

#endif // GUITABULAR_H
