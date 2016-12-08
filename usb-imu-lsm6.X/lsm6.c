#include "lsm6.h"
#include "i2c.h"

#define DS33_SA0_HIGH_ADDRESS 0b1101011
#define DS33_SA0_LOW_ADDRESS  0b1101010

#define TEST_REG_ERROR -1

#define DS33_WHO_ID 0x69

static int16_t lsm6TestReg(uint8_t address, uint8_t reg)
{
    uint8_t value = 0;
    I2CTransfer transfers[] = {
        { address, 0, &reg, 1 },
        { address, I2C_FLAG_READ, I2C_FLAG_STOP, &value, 1 },
    };
    if (i2cPerformTransfers(transfers)) { return TEST_REG_ERROR; }
    return value;
}

uint8_t lsm6Init(LSM6 * this, LSM6DeviceType device, LSM6SA0State sa0)
{
    // Detect the device type and SA0 state if needed, and also see if we
    // can talk to the device.    
    uint8_t deviceFound = 0;
    if (device == LSM6_DEVICE_TYPE_AUTO || device == LSM6_DEVICE_TYPE_DS33)
    {
        if (sa0 != LSM6_SA0_LOW && lsm6TestReg(DS33_SA0_HIGH_ADDRESS, LSM6_WHO_AM_I) == DS33_WHO_ID)
        {
            sa0 = LSM6_SA0_HIGH;
            device = LSM6_DEVICE_TYPE_DS33;
            deviceFound = 1;
        }
        
        if (sa0 != LSM6_SA0_HIGH && lsm6TestReg(DS33_SA0_LOW_ADDRESS, WHO_AM_I) == DS33_WHO_ID)
        {
            sa0 = LSM6_SA0_LOW;
            device = LSM6_DEVICE_TYPE_DS33;
            deviceFound = 1;
        }
    }

    if (!deviceFound) { return 0; }

    this->device = device;
    this->address = (sa0 == LSM6_SA0_HIGH) ? DS33_SA0_HIGH_ADDRESS : DS33_SA0_LOW_ADDRESS;

    return 1;
}

// Enables the LSM6's accelerometer and gyro. Also:
// - Sets sensor full scales (gain) to default power-on values, which are
//   +/- 2 g for accelerometer and 245 dps for gyro
// - Selects 1.66 kHz (high performance) ODR (output data rate) for
//   accelerometer and 1.66 kHz (high performance) ODR for gyro. (These are the
//   ODR settings for which the electrical characteristics are specified in the
//   datasheet.)
// - Enables automatic increment of register address during multiple byte access
//   Note that this function will also reset other settings controlled by
//   the registers it writes to.
void lsm6EnableDefault(LSM6 * this)
{
  if (this->device == LSM6_DEVICE_TYPE_DS33)
  {
    // Accelerometer

    // 0x80 = 0b10000000
    // ODR = 1000 (1.66 kHz (high performance)); FS_XL = 00 (+/-2 g full scale)
    lsm6WriteReg(LSM6_CTRL1_XL, 0x80);
    if (this->lastResult) { return; }

    // Gyro

    // 0x80 = 0b010000000
    // ODR = 1000 (1.66 kHz (high performance)); FS_XL = 00 (245 dps)
    lsm6WriteReg(LSM6_CTRL2_G, 0x80);
    if (this->lastResult) { return; }

    // Common

    // 0x04 = 0b00000100
    // IF_INC = 1 (automatically increment register address)
    lsm6WriteReg(LSM6_CTRL3_C, 0x04);
    if (this->lastResult) { return; }
  }
}

void lsm6WriteReg(LSM6 * lsm6, uint8_t reg, uint8_t value)
{
    uint8_t buffer[2] = { reg, value };
    I2CTransfer transfers[] = {
        { this->address, I2C_FLAG_STOP, &buffer, 2 },
    };

    this->lastResult = i2cPerformTransfers(transfers);
}

uint8_t lsm6ReadReg(LSM6 * this, uint8_t reg)
{
    uint8_t value = 0;
    I2CTransfer transfers[] = {
        { this->address, 0, &reg, 1 },
        { this->address, I2C_FLAG_READ, I2C_FLAG_STOP, &value, 1 },
    };
    this->lastResult = i2cPerformTransfers(transfers);
    return value;
}


// Reads all 6 channels of the LSM6 and stores them in the object variables
void lsm6Read(LSM6 * this)
{
  lsm6ReadAcc(this);
  lsm6ReadGyro(this);
}

// Reads the 3 accelerometer channels and stores them in array a.
void lsm6ReadAcc(LSM6 * this)
{
    uint8_t reg = LSM6_OUTX_L_XL;
    uint8_t buffer[6];
    I2CTransfer transfers[] = {
        { this->address, 0, &reg, 1 },
        { this->address, I2C_FLAG_READ, I2C_FLAG_STOP, &buffer, 6 },
    };
    this->lastResult = i2cPerformTransfers(transfers);
    if (this->lastResult) { return; }
    
    this->a[0] = (int16_t)(buffer[1] << 8 | buffer[0]);
    this->a[1] = (int16_t)(buffer[3] << 8 | buffer[2]);
    this->a[2] = (int16_t)(buffer[5] << 8 | buffer[4]);    
}

// Reads the 3 gyro channels and stores them in vector g
void lsm6ReadGyro(LSM6 * this)
{
    uint8_t reg = LSM6_OUTX_L_G;
    uint8_t buffer[6];
    I2CTransfer transfers[] = {
        { this->address, 0, &reg, 1 },
        { this->address, I2C_FLAG_READ, I2C_FLAG_STOP, &buffer, 6 },
    };
    this->lastResult = i2cPerformTransfers(transfers);
    if (this->lastResult) { return; }
    
    this->g[0] = (int16_t)(buffer[1] << 8 | buffer[0]);
    this->g[1] = (int16_t)(buffer[3] << 8 | buffer[2]);
    this->g[2] = (int16_t)(buffer[5] << 8 | buffer[4]); 
}

