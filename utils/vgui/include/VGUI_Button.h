//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include<VGUI.h>
#include<VGUI_Label.h>
#include<VGUI_Dar.h>

namespace vgui
{

enum MouseCode;
class ButtonController;
class ButtonGroup;
class ActionSignal;

//TODO: Button should be derived from an AbstractButton
class VGUIAPI Button : public Label
{
public:
	Button(const char* text,int x,int y,int wide,int tall);
	Button(const char* text,int x,int y);
private:
	void init();
public:
	virtual void setSelected(bool state);
	virtual void setSelectedDirect(bool state);
	virtual void setArmed(bool state);
	virtual bool isSelected();
	virtual void doClick();
	virtual void addActionSignal(ActionSignal* s);
	virtual void setButtonGroup(ButtonGroup* buttonGroup);
	virtual bool isArmed();
	virtual void setButtonBorderEnabled(bool state);
	virtual void setMouseClickEnabled(MouseCode code,bool state);
	virtual bool isMouseClickEnabled(MouseCode code);
	virtual void fireActionSignal();
	Panel* createPropertyPanel() override;
protected:
	virtual void setButtonController(ButtonController* _buttonController);
	void paintBackground() override;
protected:
	char*              _text;
	bool               _armed;
	bool               _selected;
	bool               _buttonBorderEnabled;
	Dar<ActionSignal*> _actionSignalDar;
	int                _mouseClickMask;
	ButtonGroup*       _buttonGroup;
	ButtonController*  _buttonController;
};

}
