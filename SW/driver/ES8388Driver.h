#pragma once

#include <Wire.h>

class ES8388Driver {
 public:
  enum Register : uint8_t {
    CONTROL1 = 0x00,
    CONTROL2 = 0x01,
    CHIPPOWER = 0x02,
    ADCPOWER = 0x03,
    DACPOWER = 0x04,
    MASTERMODE = 0x08,
    ADCCONTROL1 = 0x09,
    ADCCONTROL2 = 0x0A,
    ADCCONTROL3 = 0x0B,
    ADCCONTROL4 = 0x0C,
    ADCCONTROL5 = 0x0D,
    ADCCONTROL7 = 0x0F,
    ADCCONTROL8 = 0x10,
    ADCCONTROL9 = 0x11,
    DACCONTROL1 = 0x17,
    DACCONTROL2 = 0x18,
    DACCONTROL3 = 0x19,
    DACCONTROL4 = 0x1A,
    DACCONTROL16 = 0x26,
    DACCONTROL17 = 0x27,
    DACCONTROL21 = 0x2B,
    DACCONTROL24 = 0x2E,
  };

  enum class Error : uint8_t {
    None,
    InvalidArgument,
    WriteBuffer,
    WriteTimeout,
    WriteFailed,
    RegisterAddressBuffer,
    RegisterAddressTimeout,
    RegisterAddressFailed,
    ReadTimeout,
    ReadIncomplete,
    ReadFailed,
  };

  explicit ES8388Driver(TwoWire &wire, uint8_t address = 0x11)
      : wire_(wire), address_(address) {}

  bool writeRegister(uint8_t registerAddress, uint8_t value) {
    clearError();
    wire_.clearTimeoutFlag();
    wire_.beginTransmission(address_);
    const size_t addressBytesQueued = wire_.write(registerAddress);
    const size_t valueBytesQueued = wire_.write(value);
    if (addressBytesQueued != 1 || valueBytesQueued != 1) {
      wire_.endTransmission();
      return fail(Error::WriteBuffer, registerAddress);
    }

    const uint8_t status = wire_.endTransmission();
    if (wire_.getTimeoutFlag()) {
      return fail(Error::WriteTimeout, registerAddress);
    }
    if (status != 0) {
      return fail(Error::WriteFailed, registerAddress);
    }
    return true;
  }

  bool readRegister(uint8_t registerAddress, uint8_t &value) {
    clearError();
    wire_.clearTimeoutFlag();
    wire_.beginTransmission(address_);
    if (wire_.write(registerAddress) != 1) {
      wire_.endTransmission();
      return fail(Error::RegisterAddressBuffer, registerAddress);
    }

    const uint8_t status = wire_.endTransmission(false);
    if (wire_.getTimeoutFlag()) {
      return fail(Error::RegisterAddressTimeout, registerAddress);
    }
    if (status != 0) {
      return fail(Error::RegisterAddressFailed, registerAddress);
    }

    wire_.clearTimeoutFlag();
    const size_t bytesReceived = wire_.requestFrom(address_, static_cast<uint8_t>(1));
    if (wire_.getTimeoutFlag()) {
      return fail(Error::ReadTimeout, registerAddress);
    }
    if (bytesReceived != 1 || wire_.available() < 1) {
      return fail(Error::ReadIncomplete, registerAddress);
    }

    const int receivedValue = wire_.read();
    if (receivedValue < 0) {
      return fail(Error::ReadFailed, registerAddress);
    }
    value = static_cast<uint8_t>(receivedValue);
    return true;
  }

  bool configureMonoLin1ToLout1() {
    static const CodecRegisterSetting startupSettings[] = {
        {DACCONTROL3, 0x04},
        {MASTERMODE, 0x00},
        {CHIPPOWER, 0xF3},
        {CONTROL1, 0x05},
        {CONTROL2, 0x40},
        {DACPOWER, 0xC0},
        {ADCPOWER, 0xFF},
    };
    static const CodecRegisterSetting adcSettings[] = {
        {ADCCONTROL1, 0x00},
        {ADCCONTROL2, 0x00},
        {ADCCONTROL3, 0x02},
        {ADCCONTROL7, 0x30},
        {ADCCONTROL8, 0x00},
        {ADCCONTROL9, 0x00},
        {ADCCONTROL5, 0x02},
        {ADCCONTROL4, 0x0C},
        {DACCONTROL21, 0x80},
        {ADCPOWER, 0x09},
    };
    static const CodecRegisterSetting dacSettings[] = {
        {DACCONTROL4, 0x00},
        {DACCONTROL16, 0x00},
        {DACCONTROL17, 0xB8},
        {DACCONTROL2, 0x02},
        {DACCONTROL1, 0x18},
        {DACCONTROL24, 0x1E},
        {DACPOWER, 0x60},  // Power the left DAC and enable only LOUT1.
    };

    clearError();
    if (!applySettings(startupSettings,
                       sizeof(startupSettings) / sizeof(startupSettings[0])) ||
        !applySettings(adcSettings,
                       sizeof(adcSettings) / sizeof(adcSettings[0])) ||
        !applySettings(dacSettings,
                       sizeof(dacSettings) / sizeof(dacSettings[0]))) {
      return false;
    }
    return writeRegister(CHIPPOWER, 0x00);
  }

  bool setDacMuted(bool muted) {
    return writeRegister(DACCONTROL3, muted ? 0x36 : 0x32);
  }

  bool setAdcGainDb(uint8_t gainDb) {
    if (gainDb > 24 || gainDb % 3 != 0) {
      return fail(Error::InvalidArgument, ADCCONTROL1);
    }
    const uint8_t gainStep = gainDb / 3;
    return writeRegister(ADCCONTROL1,
                         static_cast<uint8_t>((gainStep << 4) | gainStep));
  }

  bool setLout1Volume(uint8_t registerValue) {
    if (registerValue > 0x21) {
      return fail(Error::InvalidArgument, DACCONTROL24);
    }
    return writeRegister(DACCONTROL24, registerValue);
  }

  uint8_t address() const { return address_; }
  Error lastError() const { return lastError_; }
  uint8_t failedRegister() const { return failedRegister_; }

  const char *lastErrorMessage() const {
    switch (lastError_) {
      case Error::None:
        return "no error";
      case Error::InvalidArgument:
        return "invalid argument";
      case Error::WriteBuffer:
        return "could not queue register write";
      case Error::WriteTimeout:
        return "I2C register write timed out";
      case Error::WriteFailed:
        return "I2C register write failed";
      case Error::RegisterAddressBuffer:
        return "could not queue register address";
      case Error::RegisterAddressTimeout:
        return "I2C register selection timed out";
      case Error::RegisterAddressFailed:
        return "I2C register selection failed";
      case Error::ReadTimeout:
        return "I2C register read timed out";
      case Error::ReadIncomplete:
        return "incomplete I2C register read";
      case Error::ReadFailed:
        return "could not retrieve register byte";
    }
    return "unknown ES8388 error";
  }

 private:
  struct CodecRegisterSetting {
    uint8_t address;
    uint8_t value;
  };

  TwoWire &wire_;
  uint8_t address_;
  Error lastError_ = Error::None;
  uint8_t failedRegister_ = 0;

  void clearError() {
    lastError_ = Error::None;
    failedRegister_ = 0;
  }

  bool fail(Error error, uint8_t registerAddress) {
    lastError_ = error;
    failedRegister_ = registerAddress;
    return false;
  }

  bool applySettings(const CodecRegisterSetting *settings, size_t count) {
    for (size_t i = 0; i < count; ++i) {
      if (!writeRegister(settings[i].address, settings[i].value)) {
        return false;
      }
    }
    return true;
  }
};
