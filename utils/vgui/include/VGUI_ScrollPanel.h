//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include<VGUI.h>
#include<VGUI_Panel.h>

//NOTE: You need to call validate anytime you change a scrollbar

namespace vgui
{

class ScrollBar;

class VGUIAPI ScrollPanel : public Panel
{
private:
	Panel*     _clientClip;
	Panel*     _client;
	ScrollBar* _horizontalScrollBar;
	ScrollBar* _verticalScrollBar;
	bool       _autoVisible[2];
public:
	ScrollPanel(int x,int y,int wide,int tall);
protected:
	void setSize(int wide,int tall) override;
public:
	virtual void   setScrollBarVisible(bool horizontal,bool vertical);
	virtual void   setScrollBarAutoVisible(bool horizontal,bool vertical);
	virtual Panel* getClient();
	virtual Panel* getClientClip();
	virtual void   setScrollValue(int horizontal,int vertical);
	virtual void   getScrollValue(int& horizontal,int& vertical);
	virtual void   recomputeClientSize();
	virtual ScrollBar* getHorizontalScrollBar();
	virtual ScrollBar* getVerticalScrollBar();
	virtual void       validate();
public: //bullshit public
	virtual void recomputeScroll();
};

}
