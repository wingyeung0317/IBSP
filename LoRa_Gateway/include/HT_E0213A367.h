#ifndef __HT_E0213A367_H__
#define __HT_E0213A367_H__

#include <HT_Display.h>
#include <SPI.h>
#include "HT_st7735_fonts.h"
SPIClass ESPI(HSPI);


class HT_E0213A367 : public ScreenDisplay
{
private:
	uint8_t _rst;
	uint8_t _dc;
	int8_t _cs;
	int8_t _clk;
	int8_t _mosi;
	int8_t _miso;
	uint32_t _freq;
	uint8_t _spi_num;
	int8_t _busy;
	uint8_t _buf[4096];
	SPISettings _spiSettings;

public:
	uint8_t _width = 250;
	uint8_t _height = 122;

	HT_E0213A367(uint8_t _rst, uint8_t _dc, int8_t _cs, int8_t _busy, int8_t _sck, int8_t _mosi, int8_t _miso, uint32_t _freq = 6000000, DISPLAY_GEOMETRY g = GEOMETRY_250_122)
	{
		setGeometry(g);
		this->_rst = _rst;
		this->_dc = _dc;
		this->_cs = _cs;
		this->_freq = _freq;
		this->_clk = _sck;
		this->_mosi = _mosi;
		this->_miso = _miso;
		this->_busy = _busy;
		this->displayType = E_INK;
	}

	bool connect()
	{
		pinMode(_dc, OUTPUT);
		pinMode(_rst, OUTPUT);
		pinMode(_cs, OUTPUT);
		digitalWrite(_cs, HIGH);
		pinMode(_busy, INPUT_PULLUP);
		this->buffer = _buf;
		ESPI.begin(this->_clk, this->_miso, this->_mosi);
		_spiSettings._clock = this->_freq;
		// Pulse Reset low for 10ms
		digitalWrite(_rst, HIGH);
		delay(100);
		digitalWrite(_rst, LOW);
		delay(100);
		digitalWrite(_rst, HIGH);
		return true;
	}

	void update(DISPLAY_BUFFER buffer)
	{
		if (buffer == BLACK_BUFFER)
		{
			updateData(0x24);
		}
		else
		{
			updateData(0x24);
		}
	}

	void display()
	{
		sendCommand(0x22);
		sendData(0xF7);  // Full refresh
		sendCommand(0x20);
		WaitUntilIdle();
	}

	void displayPartial()
	{
		sendCommand(0x22);
		sendData(0xFF);  // Partial refresh mode
		sendCommand(0x20);  // Master activation
		WaitUntilIdle();
	}

	void updateData(uint8_t addr)
	{
		int xmax = this->width();
		int ymax = this->height() >> 3;

		sendCommand(0x3C);                                                       // board
		sendData(0x01);                                                      // 0x01 border white  0x00 black  

		setFullRamArea();
		sendCommand(0x24);
		for(int x=0; x<250; x++)
		{
			for (int y = 15; y >= 0; y--)
			{
				if (y == 0)
				sendData(~(buffer[x + y * 256] << 6));
				else
				sendData(~((buffer[x + y * 256] << 6) | (buffer[x + (y - 1) * 256] >> 2)));
			}
		}
	}

	void stop()
	{
		end();
	}

	void dis_img_Partial_Refresh(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const unsigned char *img)
	{
		if (!img) return;
		
		// Set partial window
		setPartialRamArea(x, y, w, h);
		
		// Send image data to specified area
		sendCommand(0x24); // Write RAM (Black/White)
		
		int bytes = (w / 8) * h;
		for (int i = 0; i < bytes; i++) {
			sendData(img[i]);
		}
		
		// Perform partial update
		displayPartial();
	}

private:
	int getBufferOffset(void)
	{
		return 0;
	}

	void WaitUntilIdle()
	{
		while (digitalRead(_busy))
		{ // LOW: idle, HIGH: busy
		  // return;
			// Serial.println("busy");
			// esp_sleep_enable_timer_wakeup(10 * 1000);
			// esp_light_sleep_start();
			delay(10);
		}
		delay(10);
	}


	void setFullRamArea()
	{
		// Fixed full screen RAM area (original working configuration)
		sendCommand(0x11); // set ram entry mode
		sendData(0x00);    // x increase, y increase : normal mode
		sendCommand(0x44);
		sendData(0x0f);    // X end
		sendData(0);       // X start
		sendCommand(0x45);
		sendData(0xF9);    // Y end (249)
		sendData(0x00);    // Y start
		sendCommand(0x4e);
		sendData(0x0e);    // X counter
		sendCommand(0x4f);
		sendData(0xF9);    // Y counter
	}

	void setPartialRamArea(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
	{
		// For partial refresh, just use full screen area
		// The partial refresh command (0xFF) will handle the update mode
		setFullRamArea();
	}

	void sendInitCommands(void)
	{
		WaitUntilIdle();
		sendCommand(0x12); // soft reset
		WaitUntilIdle();

		sendCommand(0x01); //Driver output control
		sendData(0xF9);
		sendData(0x00);

		sendCommand(0x3C);  // Border Waveform
		sendData(0x01);

		sendCommand(0x18);
		sendData(0x80);

		sendCommand(0x37);  // Waveform ID register
		sendData(0x40); // ByteA
		sendData(0x80); // ByteB        DM[7:0]
		sendData(0x03); // ByteC        DM[[15:8]
		sendData(0x0E); // ByteD        DM[[23:16]

		setFullRamArea();
		WaitUntilIdle();
	}

	void sendScreenRotateCommand()
	{

	}

	inline void sendCommand(uint8_t com) __attribute__((always_inline))
	{
		digitalWrite(_dc, LOW);
		digitalWrite(_cs, LOW);
		ESPI.beginTransaction(_spiSettings);
		ESPI.transfer(com);
		ESPI.endTransaction();
		digitalWrite(_cs, HIGH);
		digitalWrite(_dc, HIGH);
	}
	void sendData(unsigned char data)
	{
		digitalWrite(this->_cs, LOW);
		ESPI.transfer(data);
		digitalWrite(this->_cs, HIGH);
	}
	// xx Write_data function xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
};

#endif
