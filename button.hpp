#pragma once


struct button_t {
	void press() { pressed = true;}
	void release() { pressed = false;}
	bool is_pressed() { return pressed; }
private:	
	bool pressed{false};
};
