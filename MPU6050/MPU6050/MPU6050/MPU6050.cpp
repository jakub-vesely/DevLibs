#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "MPU6050.h"


#define DEBUG_MPU
#ifdef DEBUG_MPU
    Serial debugMpu(USBTX, USBRX);
    #define LN "\r\n"
    #define BREAK debugMpu.printf(LN)
    #define PRINTF debugMpu.printf
    #define PRINTLN(x) debugMpu.printf(x);BREAK
    
#endif

#define get_ms(...)
#define min(a,b) ((a<b)?a:b)


//static int set_int_enable(uint8_t enable);








const hw_s MPU6050:: hw = {
    .addr           = MPU6050_ADDRESS,
    .max_fifo       = 1024,
    .num_reg        = 118,
    .temp_sens      = 340,
    .temp_offset    = 36.53,
    .bank_size      = 256
};

const test_s MPU6050:: test = {
    .gyro_sens      = 32768/250,
    .accel_sens     = 32768/16,
    .reg_rate_div   = 0,    /* 1kHz. */
    .reg_lpf        = 1,    /* 188Hz. */
    .reg_gyro_fsr   = 0,    /* 250dps. */
    .reg_accel_fsr  = 0x18, /* 16g. */
    .wait_ms        = 50,
    .packet_thresh  = 5,    /* 5% */
    .min_dps        = 10.f,
    .max_dps        = 105.f,
    .max_gyro_var   = 0.14f,
    .min_g          = 0.3f,
    .max_g          = 0.95f,
    .max_accel_var  = 0.14f
};

MPU6050::MPU6050():i2cDrv()//,intPin(MPU6050_DEFAULT_INT_PIN)
{
    //Set up default
    //i2c(I2C_SDA, I2C_SCL);
    //i2cFreq= 400KHz
     st = new gyro_state_s();
     st->hw = &hw;
     st->test = &test;
}

MPU6050::MPU6050(PinName i2cSda, PinName i2cScl,PinName mpu6050IntPin,uint32_t i2cFreq): i2cDrv(i2cSda,i2cScl,i2cFreq)///,intPin(mpu6050IntPin)
{

}

bool MPU6050:: writeByte(uint8_t devAddr, uint8_t regAddr, uint8_t data)
{
    return i2cDrv.write(devAddr,regAddr,1, &data);
}


bool MPU6050:: writeBytes(uint8_t devAddr, uint8_t regAddr, uint8_t dataLength, uint8_t *data)
{
    return i2cDrv.write(devAddr,regAddr,dataLength, data);
}


bool MPU6050:: readByte(uint8_t devAddr, uint8_t regAddr,uint8_t *destData)
{
    return i2cDrv.read(devAddr, regAddr, 1,destData);      
}

bool MPU6050:: readBytes(uint8_t devAddr, uint8_t regAddr, uint8_t dataLength, uint8_t * destData)
{
    return i2cDrv.read(devAddr, regAddr, dataLength,destData);
}



void MPU6050::register_int_cb(void(*intCb)())
{
   //intPin.rise(intCb);
}

/**
 *  @brief      Enable/disable data ready interrupt.
 *  If the DMP is on, the DMP interrupt is enabled. Otherwise, the data ready
 *  interrupt is used.
 *  @param[in]  enable      1 to enable interrupt.
 *  @return     0 if successful.
 */
//static int set_int_enable(uint8_t enable)
int MPU6050::set_int_enable(uint8_t enable)
{
    uint8_t tmp;

    if (st->chip_cfg.dmp_on) {
        if (enable)
            tmp = BIT_DMP_INT_EN;
        else
            tmp = 0x00;
        if (!writeBytes(MPU6050_ADDRESS, MPU6050_INT_ENABLE, 1, &tmp))
            return -1;
        st->chip_cfg.int_enable = tmp;
    } else {
        if (!st->chip_cfg.sensors)
            return -1;
        if (enable && st->chip_cfg.int_enable)
            return 0;
        if (enable)
            tmp = BIT_DATA_RDY_EN;
        else
            tmp = 0x00;
        if (!writeBytes(MPU6050_ADDRESS, MPU6050_INT_ENABLE, 1, &tmp))
            return -1;
        st->chip_cfg.int_enable = tmp;
    }
    return 0;
}
int MPU6050::mpu_reset(void)
{
    uint8_t data[6];
    data[0] = BIT_RESET;
    if (!writeBytes(MPU6050_ADDRESS, MPU6050_PWR_MGMT_1, 1, data) )
    {
        return -1;
    }
    return 0;
}

   
int MPU6050::getDeviceID()
{   
    uint8_t whoAmI; 
    readByte(MPU6050_ADDRESS, MPU6050_WHO_AM_I,&whoAmI);
    return whoAmI;
}


bool MPU6050::testConnection()
{
    char deviceId = getDeviceID();
    PRINTF("Device ID: %d 0x%X",deviceId,deviceId);
    BREAK;
    return deviceId == 0x68;
}
/**
 *  @brief      Register dump for testing.
 *  @return     0 if successful.
 */
int MPU6050::mpu_reg_dump(void)
{
    uint8_t ii;
    uint8_t data;

    for (ii = 0; ii < st->hw->num_reg; ii++) {
        if (ii == MPU6050_FIFO_R_W || ii == MPU6050_MEM_R_W)
            continue;
        if (!readBytes(MPU6050_ADDRESS, ii, 1, &data))
            return -1;
    }
    return 0;
}

/**
 *  @brief      Read from a single register.
 *  NOTE: The memory and FIFO read/write registers cannot be accessed.
 *  @param[in]  reg     Register address.
 *  @param[out] data    Register data.
 *  @return     0 if successful.
 */
int MPU6050::mpu_read_reg(uint8_t reg, uint8_t *data)
{
    if (reg == MPU6050_FIFO_R_W || reg == MPU6050_MEM_R_W)
        return -1;
    if (reg >= st->hw->num_reg)
        return -1;
    return !readBytes(MPU6050_ADDRESS, reg, 1, data);
}

/**
 *  @brief      Initialize hardware.
 *  Initial configuration:\n
 *  Gyro FSR: +/- 2000DPS\n
 *  Accel FSR +/- 2G\n
 *  DLPF: 42Hz\n
 *  FIFO rate: 50Hz\n
 *  Clock source: Gyro PLL\n
 *  FIFO: Disabled.\n
 *  Data ready interrupt: Disabled, active low, unlatched.
 *  @param[in]  int_param   Platform-specific parameters to interrupt API.
 *  @return     0 if successful.
 */
int MPU6050::mpu_init(void)
{
    uint8_t data[6];

    PRINTLN("mpu_init");
    /* Reset device. */
    data[0] = BIT_RESET;
    if (!writeByte(MPU6050_ADDRESS, MPU6050_PWR_MGMT_1, data[0] ) )
    {
        return -1;
    }
    PRINTLN("Device Reset");
    wait_ms(100);

    /* Wake up chip. */
    data[0] = 0x00;
    if (!writeBytes(MPU6050_ADDRESS, MPU6050_PWR_MGMT_1, 1, data))
        return -1;
    PRINTLN("Wake up the Chip");

   st->chip_cfg.accel_half = 0;

   /* Set to invalid values to ensure no I2C writes are skipped. */
    st->chip_cfg.sensors = 0xFF;
    st->chip_cfg.gyro_fsr = 0xFF;
    st->chip_cfg.accel_fsr = 0xFF;
    st->chip_cfg.lpf = 0xFF;
    st->chip_cfg.sample_rate = 0xFFFF;
    st->chip_cfg.fifo_enable = 0xFF;
    st->chip_cfg.bypass_mode = 0xFF;
    /* mpu_set_sensors always preserves this setting. */
    st->chip_cfg.clk_src = INV_CLK_PLL;
    /* Handled in next call to mpu_set_bypass. */
    st->chip_cfg.active_low_int = 1;
    st->chip_cfg.latched_int = 0;
    st->chip_cfg.int_motion_only = 0;
    st->chip_cfg.lp_accel_mode = 0;
    memset(&st->chip_cfg.cache, 0, sizeof(st->chip_cfg.cache));
    st->chip_cfg.dmp_on = 0;
    st->chip_cfg.dmp_loaded = 0;
    st->chip_cfg.dmp_sample_rate = 0;

    if (mpu_set_gyro_fsr(2000))
        return -1;
    PRINTLN("mpu_set_gyro_fsr");
    if (mpu_set_accel_fsr(2))
        return -1;
    PRINTLN("mpu_set_accel_fsr");
    if (mpu_set_lpf(42))
        return -1;
    PRINTLN("mpu_set_lpf");
    if (mpu_set_sample_rate(50))
        return -1;
    PRINTLN("mpu_set_sample_rate");
    if (mpu_configure_fifo(0))
        return -1;
    PRINTLN("mpu_configure_fifo");

    if (mpu_set_bypass(0))
        return -1;
        
    PRINTLN("mpu_set_compass_sample_rate");

    mpu_set_sensors(0);
    return 0;
}

/**
 *  @brief      Enter low-power accel-only mode.
 *  In low-power accel mode, the chip goes to sleep and only wakes up to sample
 *  the accelerometer at one of the following frequencies:
 *  \n MPU6050: 1.25Hz, 5Hz, 20Hz, 40Hz
 *  \n MPU6500: 1.25Hz, 2.5Hz, 5Hz, 10Hz, 20Hz, 40Hz, 80Hz, 160Hz, 320Hz, 640Hz
 *  \n If the requested rate is not one listed above, the device will be set to
 *  the next highest rate. Requesting a rate above the maximum supported
 *  frequency will result in an error.
 *  \n To select a fractional wake-up frequency, round down the value passed to
 *  @e rate.
 *  @param[in]  rate        Minimum sampling rate, or zero to disable LP
 *                          accel mode.
 *  @return     0 if successful.
 */
int MPU6050::mpu_lp_accel_mode(uint8_t rate)
{
    uint8_t tmp[2];

    if (rate > 40)
        return -1;

    if (!rate) {
        mpu_set_int_latched(0);
        tmp[0] = 0;
        tmp[1] = BIT_STBY_XYZG;
        if (!writeBytes(MPU6050_ADDRESS, MPU6050_PWR_MGMT_1, 2, tmp))
            return -1;
        st->chip_cfg.lp_accel_mode = 0;
        return 0;
    }
    /* For LP accel, we automatically configure the hardware to produce latched
     * interrupts. In LP accel mode, the hardware cycles into sleep mode before
     * it gets a chance to deassert the interrupt pin; therefore, we shift this
     * responsibility over to the MCU.
     *
     * Any register read will clear the interrupt.
     */
    mpu_set_int_latched(1);
    tmp[0] = BIT_LPA_CYCLE;
    if (rate == 1) {
        tmp[1] = INV_LPA_1_25HZ;
        mpu_set_lpf(5);
    } else if (rate <= 5) {
        tmp[1] = INV_LPA_5HZ;
        mpu_set_lpf(5);
    } else if (rate <= 20) {
        tmp[1] = INV_LPA_20HZ;
        mpu_set_lpf(10);
    } else {
        tmp[1] = INV_LPA_40HZ;
        mpu_set_lpf(20);
    }
    tmp[1] = (tmp[1] << 6) | BIT_STBY_XYZG;
    if (!writeBytes(MPU6050_ADDRESS, MPU6050_PWR_MGMT_1, 2, tmp))
        return -1;
    st->chip_cfg.sensors = INV_XYZ_ACCEL;
    st->chip_cfg.clk_src = 0;
    st->chip_cfg.lp_accel_mode = 1;
    mpu_configure_fifo(0);

    return 0;
}

/**
 *  @brief      Read raw gyro data directly from the registers.
 *  @param[out] data        Raw data in hardware units.
 *  @param[out] timestamp   Timestamp in milliseconds. Null if not needed.
 *  @return     0 if successful.
 */
int MPU6050::mpu_get_gyro_reg(int16_t *data, uint32_t *timestamp)
{
    uint8_t tmp[6];

    if (!(st->chip_cfg.sensors & INV_XYZ_GYRO))
        return -1;

    if (!readBytes(MPU6050_ADDRESS, MPU6050_GYRO_XOUT_H, 6, tmp) )
        return -1;
    data[0] = (tmp[0] << 8) | tmp[1];
    data[1] = (tmp[2] << 8) | tmp[3];
    data[2] = (tmp[4] << 8) | tmp[5];
    if (timestamp)
        get_ms(timestamp);
    return 0;
}

/**
 *  @brief      Read raw accel data directly from the registers.
 *  @param[out] data        Raw data in hardware units.
 *  @param[out] timestamp   Timestamp in milliseconds. Null if not needed.
 *  @return     0 if successful.
 */
int MPU6050::mpu_get_accel_reg(int16_t *data, uint32_t *timestamp)
{
    uint8_t tmp[6];

    if (!(st->chip_cfg.sensors & INV_XYZ_ACCEL))
        return -1;

    if (!readBytes(MPU6050_ADDRESS, MPU6050_ACCEL_XOUT_H, 6, tmp))
        return -1;
    data[0] = (tmp[0] << 8) | tmp[1];
    data[1] = (tmp[2] << 8) | tmp[3];
    data[2] = (tmp[4] << 8) | tmp[5];
    if (timestamp)
        get_ms(timestamp);
    return 0;
}

/**
 *  @brief      Read temperature data directly from the registers.
 *  @param[out] data        Data in float format.
 *  @param[out] timestamp   Timestamp in milliseconds. Null if not needed.
 *  @return     0 if successful.
 */
int MPU6050::mpu_get_temperature(float *data, uint32_t *timestamp)
{
    uint8_t tmp[2];
   int16_t raw;

    if (!(st->chip_cfg.sensors))
        return -1;
    if (!readBytes(MPU6050_ADDRESS, MPU6050_TEMP_OUT_H, 2, tmp))
        return -1;
    raw = (tmp[0] << 8) | tmp[1];
    if (timestamp)
        get_ms(timestamp);

    data[0] = (float)((float)raw/st->hw->temp_sens +st->hw->temp_offset);
    return 0;
}


/**
 *  @brief      Read biases to the accel bias 6050 registers.
 *  This function reads from the MPU6050 accel offset cancellations registers.
 *  The format are G in +-8G format. The register is initialized with OTP 
 *  factory trim values.
 *  @param[in]  accel_bias  returned structure with the accel bias
 *  @return     0 if successful.
 */
int MPU6050::mpu_read_accel_bias(int32_t *accel_bias) {
    uint8_t data[6];
    if (!readBytes(MPU6050_ADDRESS, MPU6050_XA_OFFS_H, 2, &data[0]))
        return -1;
    if (!readBytes(MPU6050_ADDRESS, MPU6050_YA_OFFS_H, 2, &data[2]))
        return -1;
    if (!readBytes(MPU6050_ADDRESS, MPU6050_ZA_OFFS_H, 2, &data[4]))
        return -1;
    accel_bias[0] = ((int32_t)data[0]<<8) | data[1];
    accel_bias[1] = ((int32_t)data[2]<<8) | data[3];
    accel_bias[2] = ((int32_t)data[4]<<8) | data[5];
    return 0;
}

int MPU6050::mpu_read_gyro_bias(int32_t *gyro_bias) {
    uint8_t data[6];
    if (!readBytes(MPU6050_ADDRESS, MPU6050_XG_OFFS_USRH, 2, &data[0]))
        return -1;
    if (!readBytes(MPU6050_ADDRESS, MPU6050_YG_OFFS_USRH, 2, &data[2]))
        return -1;
    if (!readBytes(MPU6050_ADDRESS, MPU6050_YG_OFFS_USRL, 2, &data[4]))
        return -1;
    gyro_bias[0] = ((int32_t)data[0]<<8) | data[1];
    gyro_bias[1] = ((int32_t)data[2]<<8) | data[3];
    gyro_bias[2] = ((int32_t)data[4]<<8) | data[5];
    return 0;
}

/**
 *  @brief      Push biases to the gyro bias 6500/6050 registers.
 *  This function expects biases relative to the current sensor output, and
 *  these biases will be added to the factory-supplied values. Bias inputs are LSB
 *  in +-1000dps format.
 *  @param[in]  gyro_bias  New biases.
 *  @return     0 if successful.
 */
int MPU6050::mpu_set_gyro_bias_reg(int32_t *gyro_bias)
{
    uint8_t data[6] = {0, 0, 0, 0, 0, 0};
    int i=0;
    for(i=0;i<3;i++) {
        gyro_bias[i]= (-gyro_bias[i]);
    }
    data[0] = (gyro_bias[0] >> 8) & 0xff;
    data[1] = (gyro_bias[0]) & 0xff;
    data[2] = (gyro_bias[1] >> 8) & 0xff;
    data[3] = (gyro_bias[1]) & 0xff;
    data[4] = (gyro_bias[2] >> 8) & 0xff;
    data[5] = (gyro_bias[2]) & 0xff;
    if (!writeBytes(MPU6050_ADDRESS, MPU6050_XG_OFFS_USRH, 2, &data[0]))
        return -1;
    if (!writeBytes(MPU6050_ADDRESS, MPU6050_YG_OFFS_USRH, 2, &data[2]))
        return -1;
    if (!writeBytes(MPU6050_ADDRESS, MPU6050_ZG_OFFS_USRH, 2, &data[4]))
        return -1;
    return 0;
}

/**
 *  @brief      Push biases to the accel bias 6050 registers.
 *  This function expects biases relative to the current sensor output, and
 *  these biases will be added to the factory-supplied values. Bias inputs are LSB
 *  in +-8G format.
 *  @param[in]  accel_bias  New biases.
 *  @return     0 if successful.
 */
int MPU6050::mpu_set_accel_bias_reg(const int32_t *accel_bias)
{
    uint8_t data[6] = {0, 0, 0, 0, 0, 0};
    int32_t accel_reg_bias[3] = {0, 0, 0};
    int32_t mask = 0x0001;
    uint8_t mask_bit[3] = {0, 0, 0};
    uint8_t i = 0;
    if(mpu_read_accel_bias(accel_reg_bias))
        return -1;

    //bit 0 of the 2 byte bias is for temp comp
    //calculations need to compensate for this and not change it
    for(i=0; i<3; i++) {
        if(accel_reg_bias[i]&mask)
            mask_bit[i] = 0x01;
    }

    accel_reg_bias[0] -= accel_bias[0];
    accel_reg_bias[1] -= accel_bias[1];
    accel_reg_bias[2] -= accel_bias[2];

    data[0] = (accel_reg_bias[0] >> 8) & 0xff;
    data[1] = (accel_reg_bias[0]) & 0xff;
    data[1] = data[1]|mask_bit[0];
    data[2] = (accel_reg_bias[1] >> 8) & 0xff;
    data[3] = (accel_reg_bias[1]) & 0xff;
    data[3] = data[3]|mask_bit[1];
    data[4] = (accel_reg_bias[2] >> 8) & 0xff;
    data[5] = (accel_reg_bias[2]) & 0xff;
    data[5] = data[5]|mask_bit[2];

    if (!writeBytes(MPU6050_ADDRESS, MPU6050_XA_OFFS_H, 2, &data[0]))
        return -1;
    if (!writeBytes(MPU6050_ADDRESS, MPU6050_YA_OFFS_H, 2, &data[2]))
        return -1;
    if (!writeBytes(MPU6050_ADDRESS, MPU6050_ZA_OFFS_H, 2, &data[4]))
        return -1;

    return 0;
}


/**
 *  @brief  Reset FIFO read/write pointers.
 *  @return 0 if successful.
 */
int MPU6050::mpu_reset_fifo(void)
{
    uint8_t data;

    if (!(st->chip_cfg.sensors))
        return -1;

    data = 0;
    if (!writeBytes(MPU6050_ADDRESS, MPU6050_INT_ENABLE, 1, &data))
        return -1;
    if (!writeBytes(MPU6050_ADDRESS, MPU6050_FIFO_EN, 1, &data))
        return -1;
    if (!writeBytes(MPU6050_ADDRESS, MPU6050_USER_CTRL, 1, &data))
        return -1;

    if (st->chip_cfg.dmp_on) {
        data = BIT_FIFO_RST | BIT_DMP_RST;
        if (!writeBytes(MPU6050_ADDRESS, MPU6050_USER_CTRL, 1, &data))
            return -1;
        wait_ms(1);
        data = BIT_DMP_EN | BIT_FIFO_EN;
        if (st->chip_cfg.sensors & INV_XYZ_COMPASS)
            data |= BIT_AUX_IF_EN;
        if (!writeBytes(MPU6050_ADDRESS, MPU6050_USER_CTRL, 1, &data))
            return -1;
        if (st->chip_cfg.int_enable)
            data = BIT_DMP_INT_EN;
        else
            data = 0;
        if (!writeBytes(MPU6050_ADDRESS, MPU6050_INT_ENABLE, 1, &data))
            return -1;
        data = 0;
        if (!writeBytes(MPU6050_ADDRESS, MPU6050_FIFO_EN, 1, &data))
            return -1;
    } else {
        data = BIT_FIFO_RST;
        if (!writeBytes(MPU6050_ADDRESS, MPU6050_USER_CTRL, 1, &data))
            return -1;
        if (st->chip_cfg.bypass_mode || !(st->chip_cfg.sensors & INV_XYZ_COMPASS))
            data = BIT_FIFO_EN;
        else
            data = BIT_FIFO_EN | BIT_AUX_IF_EN;
        if (!writeBytes(MPU6050_ADDRESS, MPU6050_USER_CTRL, 1, &data))
            return -1;
        wait_ms(1);
        if (st->chip_cfg.int_enable)
            data = BIT_DATA_RDY_EN;
        else
            data = 0;
        if (!writeBytes(MPU6050_ADDRESS, MPU6050_INT_ENABLE, 1, &data))
            return -1;
        if (!writeBytes(MPU6050_ADDRESS, MPU6050_FIFO_EN, 1, &st->chip_cfg.fifo_enable))
            return -1;
    }
    return 0;
}

/**
 *  @brief      Get the gyro full-scale range.
 *  @param[out] fsr Current full-scale range.
 *  @return     0 if successful.
 */
int MPU6050::mpu_get_gyro_fsr(uint16_t *fsr)
{
    switch (st->chip_cfg.gyro_fsr) {
    case INV_FSR_250DPS:
        fsr[0] = 250;
        break;
    case INV_FSR_500DPS:
        fsr[0] = 500;
        break;
    case INV_FSR_1000DPS:
        fsr[0] = 1000;
        break;
    case INV_FSR_2000DPS:
        fsr[0] = 2000;
        break;
    default:
        fsr[0] = 0;
        break;
    }
    return 0;
}

/**
 *  @brief      Set the gyro full-scale range.
 *  @param[in]  fsr Desired full-scale range.
 *  @return     0 if successful.
 */
int MPU6050::mpu_set_gyro_fsr(uint16_t fsr)
{
    uint8_t data;

    if (!(st->chip_cfg.sensors))
        return -1;

    switch (fsr) {
    case 250:
        data = INV_FSR_250DPS << 3;
        break;
    case 500:
        data = INV_FSR_500DPS << 3;
        break;
    case 1000:
        data = INV_FSR_1000DPS << 3;
        break;
    case 2000:
        data = INV_FSR_2000DPS << 3;
        break;
    default:
        return -1;
    }

    if (st->chip_cfg.gyro_fsr == (data >> 3))
        return 0;
    if (!writeBytes(MPU6050_ADDRESS, MPU6050_GYRO_CONFIG, 1, &data))
        return -1;
    st->chip_cfg.gyro_fsr = data >> 3;
    return 0;
}

/**
 *  @brief      Get the accel full-scale range.
 *  @param[out] fsr Current full-scale range.
 *  @return     0 if successful.
 */
int MPU6050::mpu_get_accel_fsr(uint8_t *fsr)
{
    switch (st->chip_cfg.accel_fsr) {
    case INV_FSR_2G:
        fsr[0] = 2;
        break;
    case INV_FSR_4G:
        fsr[0] = 4;
        break;
    case INV_FSR_8G:
        fsr[0] = 8;
        break;
    case INV_FSR_16G:
        fsr[0] = 16;
        break;
    default:
        return -1;
    }
    if (st->chip_cfg.accel_half)
        fsr[0] <<= 1;
    return 0;
}

/**
 *  @brief      Set the accel full-scale range.
 *  @param[in]  fsr Desired full-scale range.
 *  @return     0 if successful.
 */
int MPU6050::mpu_set_accel_fsr(uint8_t fsr)
{
    uint8_t data;

    if (!(st->chip_cfg.sensors))
        return -1;

    switch (fsr) {
    case 2:
        data = INV_FSR_2G << 3;
        break;
    case 4:
        data = INV_FSR_4G << 3;
        break;
    case 8:
        data = INV_FSR_8G << 3;
        break;
    case 16:
        data = INV_FSR_16G << 3;
        break;
    default:
        return -1;
    }

    if (st->chip_cfg.accel_fsr == (data >> 3))
        return 0;
    if (!writeBytes(MPU6050_ADDRESS, MPU6050_ACCEL_CONFIG, 1, &data))
        return -1;
    st->chip_cfg.accel_fsr = data >> 3;
    return 0;
}

/**
 *  @brief      Get the current DLPF setting.
 *  @param[out] lpf Current LPF setting.
 *  0 if successful.
 */
int MPU6050::mpu_get_lpf(uint16_t *lpf)
{
    switch (st->chip_cfg.lpf) {
    case INV_FILTER_188HZ:
        lpf[0] = 188;
        break;
    case INV_FILTER_98HZ:
        lpf[0] = 98;
        break;
    case INV_FILTER_42HZ:
        lpf[0] = 42;
        break;
    case INV_FILTER_20HZ:
        lpf[0] = 20;
        break;
    case INV_FILTER_10HZ:
        lpf[0] = 10;
        break;
    case INV_FILTER_5HZ:
        lpf[0] = 5;
        break;
    case INV_FILTER_256HZ_NOLPF2:
    case INV_FILTER_2100HZ_NOLPF:
    default:
        lpf[0] = 0;
        break;
    }
    return 0;
}

/**
 *  @brief      Set digital low pass filter.
 *  The following LPF settings are supported: 188, 98, 42, 20, 10, 5.
 *  @param[in]  lpf Desired LPF setting.
 *  @return     0 if successful.
 */
int MPU6050::mpu_set_lpf(uint16_t lpf)
{
    uint8_t data;

    if (!(st->chip_cfg.sensors))
        return -1;

    if (lpf >= 188)
        data = INV_FILTER_188HZ;
    else if (lpf >= 98)
        data = INV_FILTER_98HZ;
    else if (lpf >= 42)
        data = INV_FILTER_42HZ;
    else if (lpf >= 20)
        data = INV_FILTER_20HZ;
    else if (lpf >= 10)
        data = INV_FILTER_10HZ;
    else
        data = INV_FILTER_5HZ;

    if (st->chip_cfg.lpf == data)
        return 0;
    if (!writeBytes(MPU6050_ADDRESS, MPU6050_CONFIG , 1, &data))
        return -1;
    st->chip_cfg.lpf = data;
    return 0;
}

/**
 *  @brief      Get sampling rate.
 *  @param[out] rate    Current sampling rate (Hz).
 *  @return     0 if successful.
 */
int MPU6050::mpu_get_sample_rate(uint16_t *rate)
{
    if (st->chip_cfg.dmp_on)
        return -1;
    else
        rate[0] = st->chip_cfg.sample_rate;
    return 0;
}

/**
 *  @brief      Set sampling rate.
 *  Sampling rate must be between 4Hz and 1kHz.
 *  @param[in]  rate    Desired sampling rate (Hz).
 *  @return     0 if successful.
 */
int MPU6050::mpu_set_sample_rate(uint16_t rate)
{
    uint8_t data;

    if (!(st->chip_cfg.sensors))
        return -1;

    if (st->chip_cfg.dmp_on)
        return -1;
    else {
        if (st->chip_cfg.lp_accel_mode) {
            if (rate && (rate <= 40)) {
                /* Just stay in low-power accel mode. */
                mpu_lp_accel_mode(rate);
                return 0;
            }
            /* Requested rate exceeds the allowed frequencies in LP accel mode,
             * switch back to full-power mode.
             */
            mpu_lp_accel_mode(0);
        }
        if (rate < 4)
            rate = 4;
        else if (rate > 1000)
            rate = 1000;

        data = 1000 / rate - 1;
        if (!writeBytes(MPU6050_ADDRESS, MPU6050_SMPLRT_DIV, 1, &data))
            return -1;

        st->chip_cfg.sample_rate = 1000 / (1 + data);

        /* Automatically set LPF to 1/2 sampling rate. */
        mpu_set_lpf(st->chip_cfg.sample_rate >> 1);
        return 0;
    }
}


/**
 *  @brief      Get gyro sensitivity scale factor.
 *  @param[out] sens    Conversion from hardware units to dps.
 *  @return     0 if successful.
 */
int MPU6050::mpu_get_gyro_sens(float *sens)
{
    switch (st->chip_cfg.gyro_fsr) {
    case INV_FSR_250DPS:
        sens[0] = 131.f;
        break;
    case INV_FSR_500DPS:
        sens[0] = 65.5f;
        break;
    case INV_FSR_1000DPS:
        sens[0] = 32.8f;
        break;
    case INV_FSR_2000DPS:
        sens[0] = 16.4f;
        break;
    default:
        return -1;
    }
    return 0;
}

/**
 *  @brief      Get accel sensitivity scale factor.
 *  @param[out] sens    Conversion from hardware units to g's.
 *  @return     0 if successful.
 */
int MPU6050::mpu_get_accel_sens(uint16_t *sens)
{
    switch (st->chip_cfg.accel_fsr) {
    case INV_FSR_2G:
        sens[0] = 16384;
        break;
    case INV_FSR_4G:
        sens[0] = 8092;
        break;
    case INV_FSR_8G:
        sens[0] = 4096;
        break;
    case INV_FSR_16G:
        sens[0] = 2048;
        break;
    default:
        return -1;
    }
    if (st->chip_cfg.accel_half)
        sens[0] >>= 1;
    return 0;
}

/**
 *  @brief      Get current FIFO configuration.
 *  @e sensors can contain a combination of the following flags:
 *  \n INV_X_GYRO, INV_Y_GYRO, INV_Z_GYRO
 *  \n INV_XYZ_GYRO
 *  \n INV_XYZ_ACCEL
 *  @param[out] sensors Mask of sensors in FIFO.
 *  @return     0 if successful.
 */
int MPU6050::mpu_get_fifo_config(uint8_t *sensors)
{
    sensors[0] = st->chip_cfg.fifo_enable;
    return 0;
}

/**
 *  @brief      Select which sensors are pushed to FIFO.
 *  @e sensors can contain a combination of the following flags:
 *  \n INV_X_GYRO, INV_Y_GYRO, INV_Z_GYRO
 *  \n INV_XYZ_GYRO
 *  \n INV_XYZ_ACCEL
 *  @param[in]  sensors Mask of sensors to push to FIFO.
 *  @return     0 if successful.
 */
int MPU6050::mpu_configure_fifo(uint8_t sensors)
{
    uint8_t prev;
    int result = 0;

    /* Compass data isn't going into the FIFO. Stop trying. */
    sensors &= ~INV_XYZ_COMPASS;

    if (st->chip_cfg.dmp_on)
        return 0;
    else {
        if (!(st->chip_cfg.sensors))
            return -1;
        prev = st->chip_cfg.fifo_enable;
        st->chip_cfg.fifo_enable = sensors & st->chip_cfg.sensors;
        if (st->chip_cfg.fifo_enable != sensors)
            /* You're not getting what you asked for. Some sensors are
             * asleep.
             */
            result = -1;
        else
            result = 0;
        if (sensors || st->chip_cfg.lp_accel_mode)
            set_int_enable(1);
        else
            set_int_enable(0);
        if (sensors) {
            if (mpu_reset_fifo()) {
                st->chip_cfg.fifo_enable = prev;
                return -1;
            }
        }
    }

    return result;
}

/**
 *  @brief      Get current power state.
 *  @param[in]  power_on    1 if turned on, 0 if suspended.
 *  @return     0 if successful.
 */
int MPU6050::mpu_get_power_state(uint8_t *power_on)
{
    if (st->chip_cfg.sensors)
        power_on[0] = 1;
    else
        power_on[0] = 0;
    return 0;
}

/**
 *  @brief      Turn specific sensors on/off.
 *  @e sensors can contain a combination of the following flags:
 *  \n INV_X_GYRO, INV_Y_GYRO, INV_Z_GYRO
 *  \n INV_XYZ_GYRO
 *  \n INV_XYZ_ACCEL
 *  \n INV_XYZ_COMPASS
 *  @param[in]  sensors    Mask of sensors to wake.
 *  @return     0 if successful.
 */
int MPU6050::mpu_set_sensors(uint8_t sensors)
{
//    PRINTLN("\t mpu_set_sensors");
    uint8_t data;

    if (sensors & INV_XYZ_GYRO)
        data = INV_CLK_PLL;
    else if (sensors)
        data = 0;
    else
        data = BIT_SLEEP;
    if (!writeBytes(MPU6050_ADDRESS, MPU6050_PWR_MGMT_1, 1, &data)) {
        st->chip_cfg.sensors = 0;
        return -1;
    }
    st->chip_cfg.clk_src = data & ~BIT_SLEEP;

    data = 0;
    if (!(sensors & INV_X_GYRO))
        data |= BIT_STBY_XG;
    if (!(sensors & INV_Y_GYRO))
        data |= BIT_STBY_YG;
    if (!(sensors & INV_Z_GYRO))
        data |= BIT_STBY_ZG;
    if (!(sensors & INV_XYZ_ACCEL))
        data |= BIT_STBY_XYZA;
    if (!writeBytes(MPU6050_ADDRESS, MPU6050_PWR_MGMT_2, 1, &data)) {
        st->chip_cfg.sensors = 0;
        return -1;
    }

//    PRINTLN("\t Latched interrupts");
    if (sensors && (sensors != INV_XYZ_ACCEL))
        /* Latched interrupts only used in LP accel mode. */
        mpu_set_int_latched(0);

    st->chip_cfg.sensors = sensors;
    st->chip_cfg.lp_accel_mode = 0;
    wait_ms(20);
    return 0;
}

/**
 *  @brief      Read the MPU interrupt status registers.
 *  @param[out] status  Mask of interrupt bits.
 *  @return     0 if successful.
 */
int MPU6050::mpu_get_int_status(int16_t *status)
{
    uint8_t tmp[2];
    if (!st->chip_cfg.sensors)
        return -1;
    if (!readBytes(MPU6050_ADDRESS, MPU6050_DMP_INT_STATUS, 2, tmp))
        return -1;
    status[0] = (tmp[0] << 8) | tmp[1];
    return 0;
}

/**
 *  @brief      Get one packet from the FIFO.
 *  If @e sensors does not contain a particular sensor, disregard the data
 *  returned to that pointer.
 *  \n @e sensors can contain a combination of the following flags:
 *  \n INV_X_GYRO, INV_Y_GYRO, INV_Z_GYRO
 *  \n INV_XYZ_GYRO
 *  \n INV_XYZ_ACCEL
 *  \n If the FIFO has no new data, @e sensors will be zero.
 *  \n If the FIFO is disabled, @e sensors will be zero and this function will
 *  return a non-zero error code.
 *  @param[out] gyro        Gyro data in hardware units.
 *  @param[out] accel       Accel data in hardware units.
 *  @param[out] timestamp   Timestamp in milliseconds.
 *  @param[out] sensors     Mask of sensors read from FIFO.
 *  @param[out] more        Number of remaining packets.
 *  @return     0 if successful.
 */
int MPU6050::mpu_read_fifo(int16_t *gyro,int16_t *accel, uint32_t *timestamp,
        uint8_t *sensors, uint8_t *more)
{
    /* Assumes maximum packet size is gyro (6) + accel (6). */
    uint8_t data[MAX_PACKET_LENGTH];
    uint8_t packet_size = 0;
    uint16_t fifo_count, index = 0;

    if (st->chip_cfg.dmp_on)
        return -1;

    sensors[0] = 0;
    if (!st->chip_cfg.sensors)
        return -1;
    if (!st->chip_cfg.fifo_enable)
        return -1;

    if (st->chip_cfg.fifo_enable & INV_X_GYRO)
        packet_size += 2;
    if (st->chip_cfg.fifo_enable & INV_Y_GYRO)
        packet_size += 2;
    if (st->chip_cfg.fifo_enable & INV_Z_GYRO)
        packet_size += 2;
    if (st->chip_cfg.fifo_enable & INV_XYZ_ACCEL)
        packet_size += 6;

    if (!readBytes(MPU6050_ADDRESS, MPU6050_FIFO_COUNTH, 2, data))
        return -1;
    fifo_count = (data[0] << 8) | data[1];
    if (fifo_count < packet_size)
        return 0;
//    log_i("FIFO count: %hd\n", fifo_count);
    if (fifo_count > (st->hw->max_fifo >> 1)) {
        /* FIFO is 50% full, better check overflow bit. */
        if (!readBytes(MPU6050_ADDRESS, MPU6050_INT_STATUS, 1, data))
            return -1;
        if (data[0] & BIT_FIFO_OVERFLOW) {
            mpu_reset_fifo();
            return -2;
        }
    }
    get_ms((uint32_t*)timestamp);

    if (!readBytes(MPU6050_ADDRESS, MPU6050_FIFO_R_W, packet_size, data))
        return -1;
    more[0] = fifo_count / packet_size - 1;
    sensors[0] = 0;

    if ((index != packet_size) && st->chip_cfg.fifo_enable & INV_XYZ_ACCEL) {
        accel[0] = (data[index+0] << 8) | data[index+1];
        accel[1] = (data[index+2] << 8) | data[index+3];
        accel[2] = (data[index+4] << 8) | data[index+5];
        sensors[0] |= INV_XYZ_ACCEL;
        index += 6;
    }
    if ((index != packet_size) && st->chip_cfg.fifo_enable & INV_X_GYRO) {
        gyro[0] = (data[index+0] << 8) | data[index+1];
        sensors[0] |= INV_X_GYRO;
        index += 2;
    }
    if ((index != packet_size) && st->chip_cfg.fifo_enable & INV_Y_GYRO) {
        gyro[1] = (data[index+0] << 8) | data[index+1];
        sensors[0] |= INV_Y_GYRO;
        index += 2;
    }
    if ((index != packet_size) && st->chip_cfg.fifo_enable & INV_Z_GYRO) {
        gyro[2] = (data[index+0] << 8) | data[index+1];
        sensors[0] |= INV_Z_GYRO;
        index += 2;
    }

    return 0;
}

/**
 *  @brief      Get one unparsed packet from the FIFO.
 *  This function should be used if the packet is to be parsed elsewhere.
 *  @param[in]  length  Length of one FIFO packet.
 *  @param[in]  data    FIFO packet.
 *  @param[in]  more    Number of remaining packets.
 */
int MPU6050::mpu_read_fifo_stream(uint16_t length, uint8_t *data,
    uint8_t *more)
{
    uint8_t tmp[2];
    uint16_t fifo_count;
    if (!st->chip_cfg.dmp_on)
        return -1;
    if (!st->chip_cfg.sensors)
        return -1;
    
    if (!readBytes(MPU6050_ADDRESS, MPU6050_FIFO_COUNTH, 2, tmp))
        return -1;
//    PRINTF("\tFIFO Count done: %d \n\r",fifo_count);
    fifo_count = ((uint16_t) tmp[0] << 8) | tmp[1];
    if (fifo_count < length) {
        more[0] = 0;
        return -2;
    }
//    PRINTLN("\tFifo Count checked");
    if (fifo_count > (st->hw->max_fifo >> 1)) {
        /* FIFO is 50% full, better check overflow bit. */
        if (!readBytes(MPU6050_ADDRESS, MPU6050_INT_STATUS, 1, tmp))
            return -1;
        if (tmp[0] & BIT_FIFO_OVERFLOW) {
            mpu_reset_fifo();
            return -3;
        }
    }
//    PRINTLN("\tCheck overflow done");

    if (!readBytes(MPU6050_ADDRESS, MPU6050_FIFO_R_W, length, data))
        return -1;
    more[0] = fifo_count / length - 1;
//    PRINTLN("\tFifo read Done");
    return 0;
}

/**
 *  @brief      Set device to bypass mode.
 *  @param[in]  bypass_on   1 to enable bypass mode.
 *  @return     0 if successful.
 */
int MPU6050::mpu_set_bypass(uint8_t bypass_on)
{
    uint8_t tmp;

    if (st->chip_cfg.bypass_mode == bypass_on)
        return 0;

    if (bypass_on) {
        if (!readBytes(MPU6050_ADDRESS, MPU6050_USER_CTRL, 1, &tmp))
            return -1;
        tmp &= ~BIT_AUX_IF_EN;
        if (!writeBytes(MPU6050_ADDRESS, MPU6050_USER_CTRL, 1, &tmp))
            return -1;
        wait_ms(3);
        tmp = BIT_BYPASS_EN;
        if (st->chip_cfg.active_low_int)
            tmp |= BIT_ACTL;
        if (st->chip_cfg.latched_int)
            tmp |= BIT_LATCH_EN | BIT_ANY_RD_CLR;
        if (!writeBytes(MPU6050_ADDRESS, MPU6050_INT_PIN_CFG, 1, &tmp))
            return -1;
    } else {
        /* Enable I2C master mode if compass is being used. */
        if (!readBytes(MPU6050_ADDRESS, MPU6050_USER_CTRL, 1, &tmp))
            return -1;
        if (st->chip_cfg.sensors & INV_XYZ_COMPASS)
            tmp |= BIT_AUX_IF_EN;
        else
            tmp &= ~BIT_AUX_IF_EN;
        if (!writeBytes(MPU6050_ADDRESS, MPU6050_USER_CTRL, 1, &tmp))
            return -1;
        wait_ms(3);
        if (st->chip_cfg.active_low_int)
            tmp = BIT_ACTL;
        else
            tmp = 0;
        if (st->chip_cfg.latched_int)
            tmp |= BIT_LATCH_EN | BIT_ANY_RD_CLR;
        if (!writeBytes(MPU6050_ADDRESS, MPU6050_INT_PIN_CFG, 1, &tmp))
            return -1;
    }
    st->chip_cfg.bypass_mode = bypass_on;
    return 0;
}

/**
 *  @brief      Set interrupt level.
 *  @param[in]  active_low  1 for active low, 0 for active high.
 *  @return     0 if successful.
 */
int MPU6050::mpu_set_int_level(uint8_t active_low)
{
    st->chip_cfg.active_low_int = active_low;
    return 0;
}

/**
 *  @brief      Enable latched interrupts.
 *  Any MPU register will clear the interrupt.
 *  @param[in]  enable  1 to enable, 0 to disable.
 *  @return     0 if successful.
 */
int MPU6050::mpu_set_int_latched(uint8_t enable)
{
    uint8_t tmp;
    if (st->chip_cfg.latched_int == enable)
        return 0;

    if (enable)
        tmp = BIT_LATCH_EN | BIT_ANY_RD_CLR;
    else
        tmp = 0;
    if (st->chip_cfg.bypass_mode)
        tmp |= BIT_BYPASS_EN;
    if (st->chip_cfg.active_low_int)
        tmp |= BIT_ACTL;
    if (!writeBytes(MPU6050_ADDRESS, MPU6050_INT_PIN_CFG, 1, &tmp))
        return -1;
    st->chip_cfg.latched_int = enable;
    return 0;
}


int MPU6050::get_accel_prod_shift(float *st_shift)
{
    uint8_t tmp[4], shift_code[3], ii;

    if (!readBytes(MPU6050_ADDRESS, 0x0D, 4, tmp))
        return 0x07;

    shift_code[0] = ((tmp[0] & 0xE0) >> 3) | ((tmp[3] & 0x30) >> 4);
    shift_code[1] = ((tmp[1] & 0xE0) >> 3) | ((tmp[3] & 0x0C) >> 2);
    shift_code[2] = ((tmp[2] & 0xE0) >> 3) | (tmp[3] & 0x03);
    for (ii = 0; ii < 3; ii++) {
        if (!shift_code[ii]) {
            st_shift[ii] = 0.f;
            continue;
        }
        /* Equivalent to..
         * st_shift[ii] = 0.34f * powf(0.92f/0.34f, (shift_code[ii]-1) / 30.f)
         */
        st_shift[ii] = 0.34f;
        while (--shift_code[ii])
            st_shift[ii] *= 1.034f;
    }
    return 0;
}

int MPU6050::accel_self_test(int32_t *bias_regular, int32_t *bias_st)
{
    int jj, result = 0;
    float st_shift[3], st_shift_cust, st_shift_var;

    get_accel_prod_shift(st_shift);
    for(jj = 0; jj < 3; jj++) {
        st_shift_cust = labs(bias_regular[jj] - bias_st[jj]) / 65536.f;
        if (st_shift[jj]) {
            st_shift_var = st_shift_cust / st_shift[jj] - 1.f;
            if (fabs(st_shift_var) > st->test->max_accel_var)
                result |= 1 << jj;
        } else if ((st_shift_cust < st->test->min_g) ||
            (st_shift_cust > st->test->max_g))
            result |= 1 << jj;
    }

    return result;
}

int MPU6050::gyro_self_test(int32_t *bias_regular, int32_t *bias_st)
{
    int jj, result = 0;
    uint8_t tmp[3];
    float st_shift, st_shift_cust, st_shift_var;

    if (!readBytes(MPU6050_ADDRESS, 0x0D, 3, tmp))
        return 0x07;

    tmp[0] &= 0x1F;
    tmp[1] &= 0x1F;
    tmp[2] &= 0x1F;

    for (jj = 0; jj < 3; jj++) {
        st_shift_cust = labs(bias_regular[jj] - bias_st[jj]) / 65536.f;
        if (tmp[jj]) {
            st_shift = 3275.f / st->test->gyro_sens;
            while (--tmp[jj])
                st_shift *= 1.046f;
            st_shift_var = st_shift_cust / st_shift - 1.f;
            if (fabs(st_shift_var) > st->test->max_gyro_var)
                result |= 1 << jj;
        } else if ((st_shift_cust < st->test->min_dps) ||
            (st_shift_cust > st->test->max_dps))
            result |= 1 << jj;
    }
    return result;
}



int MPU6050::get_st_biases(int32_t *gyro, int32_t *accel, uint8_t hw_test)
{
    uint8_t data[MAX_PACKET_LENGTH];
    uint8_t packet_count, ii;
    uint16_t fifo_count;

    data[0] = 0x01;
    data[1] = 0;
    if (!writeBytes(MPU6050_ADDRESS, MPU6050_PWR_MGMT_1, 2, data))
        return -1;
    wait_ms(200);
    data[0] = 0;
    if (!writeBytes(MPU6050_ADDRESS, MPU6050_INT_ENABLE, 1, data))
        return -1;
    if (!writeBytes(MPU6050_ADDRESS, MPU6050_FIFO_EN, 1, data))
        return -1;
    if (!writeBytes(MPU6050_ADDRESS, MPU6050_PWR_MGMT_1, 1, data))
        return -1;
    if (!writeBytes(MPU6050_ADDRESS, MPU6050_I2C_MST_CTRL, 1, data))
        return -1;
    if (!writeBytes(MPU6050_ADDRESS, MPU6050_USER_CTRL, 1, data))
        return -1;
    data[0] = BIT_FIFO_RST | BIT_DMP_RST;
    if (!writeBytes(MPU6050_ADDRESS, MPU6050_USER_CTRL, 1, data))
        return -1;
    wait_ms(15);
    data[0] = st->test->reg_lpf;
    if (!writeBytes(MPU6050_ADDRESS, MPU6050_CONFIG , 1, data))
        return -1;
    data[0] = st->test->reg_rate_div;
    if (!writeBytes(MPU6050_ADDRESS, MPU6050_SMPLRT_DIV, 1, data))
        return -1;
    if (hw_test)
        data[0] = st->test->reg_gyro_fsr | 0xE0;
    else
        data[0] = st->test->reg_gyro_fsr;
    if (!writeBytes(MPU6050_ADDRESS, MPU6050_GYRO_CONFIG, 1, data))
        return -1;

    if (hw_test)
        data[0] = st->test->reg_accel_fsr | 0xE0;
    else
        data[0] = st->test->reg_accel_fsr;
    if (!writeBytes(MPU6050_ADDRESS, MPU6050_ACCEL_CONFIG, 1, data))
        return -1;
    if (hw_test)
        wait_ms(200);

    /* Fill FIFO for test->wait_ms milliseconds. */
    data[0] = BIT_FIFO_EN;
    if (!writeBytes(MPU6050_ADDRESS, MPU6050_USER_CTRL, 1, data))
        return -1;

    data[0] = INV_XYZ_GYRO | INV_XYZ_ACCEL;
    if (!writeBytes(MPU6050_ADDRESS, MPU6050_FIFO_EN, 1, data))
        return -1;
    wait_ms(st->test->wait_ms);
    data[0] = 0;
    if (!writeBytes(MPU6050_ADDRESS, MPU6050_FIFO_EN, 1, data))
        return -1;

    if (!readBytes(MPU6050_ADDRESS, MPU6050_FIFO_COUNTH, 2, data))
        return -1;

    fifo_count = (data[0] << 8) | data[1];
    packet_count = fifo_count / MAX_PACKET_LENGTH;
    gyro[0] = gyro[1] = gyro[2] = 0;
    accel[0] = accel[1] = accel[2] = 0;

    for (ii = 0; ii < packet_count; ii++) {
       int16_t accel_cur[3], gyro_cur[3];
        if (!readBytes(MPU6050_ADDRESS, MPU6050_FIFO_R_W, MAX_PACKET_LENGTH, data))
            return -1;
        accel_cur[0] = ((int16_t)data[0] << 8) | data[1];
        accel_cur[1] = ((int16_t)data[2] << 8) | data[3];
        accel_cur[2] = ((int16_t)data[4] << 8) | data[5];
        accel[0] += (int32_t)accel_cur[0];
        accel[1] += (int32_t)accel_cur[1];
        accel[2] += (int32_t)accel_cur[2];
        gyro_cur[0] = (((int16_t)data[6] << 8) | data[7]);
        gyro_cur[1] = (((int16_t)data[8] << 8) | data[9]);
        gyro_cur[2] = (((int16_t)data[10] << 8) | data[11]);
        gyro[0] += (int32_t)gyro_cur[0];
        gyro[1] += (int32_t)gyro_cur[1];
        gyro[2] += (int32_t)gyro_cur[2];
    }
#ifdef EMPL_NO_64BIT
    gyro[0] = (int32_t)(((float)gyro[0]*65536.f) /  st->
    vtest->gyro_sens / packet_count);
    gyro[1] = (int32_t)(((float)gyro[1]*65536.f) /  st->test->gyro_sens / packet_count);
    gyro[2] = (int32_t)(((float)gyro[2]*65536.f) /  st->test->gyro_sens / packet_count);
    if (has_accel) {
        accel[0] = (int32_t)(((float)accel[0]*65536.f) /  st->test->accel_sens /
            packet_count);
        accel[1] = (int32_t)(((float)accel[1]*65536.f) /  st->test->accel_sens /
            packet_count);
        accel[2] = (int32_t)(((float)accel[2]*65536.f) /  st->test->accel_sens /
            packet_count);
        /* Don't remove gravity! */
        accel[2] -= 65536L;
    }
#else
    gyro[0] = (int32_t)(((int64_t)gyro[0]<<16) / st->test->gyro_sens / packet_count);
    gyro[1] = (int32_t)(((int64_t)gyro[1]<<16) / st->test->gyro_sens / packet_count);
    gyro[2] = (int32_t)(((int64_t)gyro[2]<<16) / st->test->gyro_sens / packet_count);
    accel[0] = (int32_t)(((int64_t)accel[0]<<16) / st->test->accel_sens /
        packet_count);
    accel[1] = (int32_t)(((int64_t)accel[1]<<16) / st->test->accel_sens /
        packet_count);
    accel[2] = (int32_t)(((int64_t)accel[2]<<16) / st->test->accel_sens /
        packet_count);
    /* Don't remove gravity! */
    if (accel[2] > 0L)
        accel[2] -= 65536L;
    else
        accel[2] += 65536L;
#endif

    return 0;
}



 /*
 *  \n This function must be called with the device either face-up or face-down
 *  (z-axis is parallel to gravity).
 *  @param[out] gyro        Gyro biases in q16 format.
 *  @param[out] accel       Accel biases (if applicable) in q16 format.
 *  @return     Result mask (see above).
 */
int MPU6050::mpu_run_self_test(int32_t *gyro, int32_t *accel)
{
    const uint8_t tries = 2;
    int32_t gyro_st[3], accel_st[3];
    uint8_t accel_result, gyro_result;
    int ii;
    int result;
    uint8_t accel_fsr, fifo_sensors, sensors_on;
    uint16_t gyro_fsr, sample_rate, lpf;
    uint8_t dmp_was_on;

    if (st->chip_cfg.dmp_on) {
        mpu_set_dmp_state(0);
        dmp_was_on = 1;
    } else
        dmp_was_on = 0;

    /* Get initial settings. */
    mpu_get_gyro_fsr(&gyro_fsr);
    mpu_get_accel_fsr(&accel_fsr);
    mpu_get_lpf(&lpf);
    mpu_get_sample_rate(&sample_rate);
    sensors_on = st->chip_cfg.sensors;
    mpu_get_fifo_config(&fifo_sensors);

    /* For older chips, the self-test will be different. */
    for (ii = 0; ii < tries; ii++)
        if (!get_st_biases(gyro, accel, 0))
            break;
    if (ii == tries) {
        /* If we reach this point, we most likely encountered an I2C error.
         * We'll just report an error for all three sensors.
         */
        result = 0;
        goto restore;
    }
    for (ii = 0; ii < tries; ii++)
        if (!get_st_biases(gyro_st, accel_st, 1))
            break;
    if (ii == tries) {
        /* Again, probably an I2C error. */
        result = 0;
        goto restore;
    }
    accel_result = accel_self_test(accel, accel_st);
    gyro_result = gyro_self_test(gyro, gyro_st);

    result = 0;
    if (!gyro_result)
        result |= 0x01;
    if (!accel_result)
        result |= 0x02;
    result |= 0x04;

restore:

    /* Set to invalid values to ensure no I2C writes are skipped. */
    st->chip_cfg.gyro_fsr = 0xFF;
    st->chip_cfg.accel_fsr = 0xFF;
    st->chip_cfg.lpf = 0xFF;
    st->chip_cfg.sample_rate = 0xFFFF;
    st->chip_cfg.sensors = 0xFF;
    st->chip_cfg.fifo_enable = 0xFF;
    st->chip_cfg.clk_src = INV_CLK_PLL;
    mpu_set_gyro_fsr(gyro_fsr);
    mpu_set_accel_fsr(accel_fsr);
    mpu_set_lpf(lpf);
    mpu_set_sample_rate(sample_rate);
    mpu_set_sensors(sensors_on);
    mpu_configure_fifo(fifo_sensors);

    if (dmp_was_on)
        mpu_set_dmp_state(1);

    return result;
}

/**
 *  @brief      Write to the DMP memory.
 *  This function prevents I2C writes past the bank boundaries. The DMP memory
 *  is only accessible when the chip is awake.
 *  @param[in]  mem_addr    Memory location (bank << 8 | start address)
 *  @param[in]  length      Number of bytes to write.
 *  @param[in]  data        Bytes to write to memory.
 *  @return     0 if successful.
 */
int MPU6050::mpu_write_mem(uint16_t mem_addr, uint16_t length,
        uint8_t *data)
{
    uint8_t tmp[2];

    if (!data)
        return -1;
    if (!st->chip_cfg.sensors)
        return -1;

    tmp[0] = (uint8_t)(mem_addr >> 8);
    tmp[1] = (uint8_t)(mem_addr & 0xFF);

    /* Check bank boundaries. */
    if (tmp[1] + length > st->hw->bank_size)
        return -1;

    if (!writeBytes(MPU6050_ADDRESS, MPU6050_BANK_SEL, 2, tmp))
        return -1;
    if (!writeBytes(MPU6050_ADDRESS, MPU6050_MEM_R_W, length, data))
        return -1;
    return 0;
}

/**
 *  @brief      Read from the DMP memory.
 *  This function prevents I2C reads past the bank boundaries. The DMP memory
 *  is only accessible when the chip is awake.
 *  @param[in]  mem_addr    Memory location (bank << 8 | start address)
 *  @param[in]  length      Number of bytes to read.
 *  @param[out] data        Bytes read from memory.
 *  @return     0 if successful.
 */
int MPU6050::mpu_read_mem(uint16_t mem_addr, uint16_t length,
        uint8_t *data)
{
    uint8_t tmp[2];

    if (!data)
        return -1;
    if (!st->chip_cfg.sensors)
        return -1;

    tmp[0] = (uint8_t)(mem_addr >> 8);
    tmp[1] = (uint8_t)(mem_addr & 0xFF);

    /* Check bank boundaries. */
    if (tmp[1] + length > st->hw->bank_size)
        return -1;

    if (!writeBytes(MPU6050_ADDRESS, MPU6050_BANK_SEL, 2, tmp))
        return -1;
    if (!readBytes(MPU6050_ADDRESS, MPU6050_MEM_R_W, length, data))
        return -1;
    return 0;
}

/**
 *  @brief      Load and verify DMP image.
 *  @param[in]  length      Length of DMP image.
 *  @param[in]  firmware    DMP code.
 *  @param[in]  start_addr  Starting address of DMP code memory.
 *  @param[in]  sample_rate Fixed sampling rate used when DMP is enabled.
 *  @return     0 if successful.
 */
int MPU6050::mpu_load_firmware(uint16_t length, const uint8_t *firmware,
    uint16_t start_addr, uint16_t sample_rate)
{
    uint16_t ii;
    uint16_t this_write;
    /* Must divide evenly into st->hw->bank_size to avoid bank crossings. */
#define LOAD_CHUNK  (16)
    uint8_t cur[LOAD_CHUNK], tmp[2];

    if (st->chip_cfg.dmp_loaded)
        /* DMP should only be loaded once. */
        return -1;

    if (!firmware)
        return -1;
    for (ii = 0; ii < length; ii += this_write) {
        this_write = min(LOAD_CHUNK, length - ii);
        if (mpu_write_mem(ii, this_write, (uint8_t*)&firmware[ii]))
            return -1;
        if (mpu_read_mem(ii, this_write, cur))
            return -1;
        if (memcmp(firmware+ii, cur, this_write))
            return -2;
    }

    /* Set program start address. */
    tmp[0] = start_addr >> 8;
    tmp[1] = start_addr & 0xFF;
    if (!writeBytes(MPU6050_ADDRESS, MPU6050_DMP_CFG_1, 2, tmp))
        return -1;

    st->chip_cfg.dmp_loaded = 1;
    st->chip_cfg.dmp_sample_rate = sample_rate;
    return 0;
}

/**
 *  @brief      Enable/disable DMP support.
 *  @param[in]  enable  1 to turn on the DMP.
 *  @return     0 if successful.
 */
int MPU6050::mpu_set_dmp_state(uint8_t enable)
{
    uint8_t tmp;
    if (st->chip_cfg.dmp_on == enable)
        return 0;

    if (enable) {
        if (!st->chip_cfg.dmp_loaded)
            return -1;
        /* Disable data ready interrupt. */
        set_int_enable(0);
        /* Disable bypass mode. */
        mpu_set_bypass(0);
        /* Keep constant sample rate, FIFO rate controlled by DMP. */
        mpu_set_sample_rate(st->chip_cfg.dmp_sample_rate);
        /* Remove FIFO elements. */
        tmp = 0;
        writeBytes(MPU6050_ADDRESS, 0x23, 1, &tmp);
        st->chip_cfg.dmp_on = 1;
        /* Enable DMP interrupt. */
        set_int_enable(1);
        mpu_reset_fifo();
    } else {
        /* Disable DMP interrupt. */
        set_int_enable(0);
        /* Restore FIFO settings. */
        tmp = st->chip_cfg.fifo_enable;
        writeBytes(MPU6050_ADDRESS, 0x23, 1, &tmp);
        st->chip_cfg.dmp_on = 0;
        mpu_reset_fifo();
    }
    return 0;
}

/**
 *  @brief      Get DMP state.
 *  @param[out] enabled 1 if enabled.
 *  @return     0 if successful.
 */
int MPU6050::mpu_get_dmp_state(uint8_t *enabled)
{
    enabled[0] = st->chip_cfg.dmp_on;
    return 0;
}




/**
 *  @brief      Enters LP accel motion interrupt mode.
 *  The behaviour of this feature is very different between the MPU6050 and the
 *  MPU6500. Each chip's version of this feature is explained below.
 *
 *  \n The hardware motion threshold can be between 32mg and 8160mg in 32mg
 *  increments.
 *
 *  \n Low-power accel mode supports the following frequencies:
 *  \n 1.25Hz, 5Hz, 20Hz, 40Hz
 *
 *  \n MPU6500:
 *  \n Unlike the MPU6050 version, the hardware does not "lock in" a reference
 *  sample. The hardware monitors the accel data and detects any large change
 *  over aint16_t period of time.
 *
 *  \n The hardware motion threshold can be between 4mg and 1020mg in 4mg
 *  increments.
 *
 *  \n MPU6500 Low-power accel mode supports the following frequencies:
 *  \n 1.25Hz, 2.5Hz, 5Hz, 10Hz, 20Hz, 40Hz, 80Hz, 160Hz, 320Hz, 640Hz
 *
 *  \n\n NOTES:
 *  \n The driver will round down @e thresh to the nearest supported value if
 *  an unsupported threshold is selected.
 *  \n To select a fractional wake-up frequency, round down the value passed to
 *  @e lpa_freq.
 *  \n The MPU6500 does not support a delay parameter. If this function is used
 *  for the MPU6500, the value passed to @e time will be ignored.
 *  \n To disable this mode, set @e lpa_freq to zero. The driver will restore
 *  the previous configuration.
 *
 *  @param[in]  thresh      Motion threshold in mg.
 *  @param[in]  time        Duration in milliseconds that the accel data must
 *                          exceed @e thresh before motion is reported.
 *  @param[in]  lpa_freq    Minimum sampling rate, or zero to disable.
 *  @return     0 if successful.
 */
int MPU6050::mpu_lp_motion_interrupt(uint16_t thresh, uint8_t time,
    uint8_t lpa_freq)
{

    if (lpa_freq) {

        if (!time)
            /* Minimum duration must be 1ms. */
            time = 1;

        if (!st->chip_cfg.int_motion_only) {
            /* Store current settings for later. */
            if (st->chip_cfg.dmp_on) {
                mpu_set_dmp_state(0);
                st->chip_cfg.cache.dmp_on = 1;
            } else
                st->chip_cfg.cache.dmp_on = 0;
            mpu_get_gyro_fsr(&st->chip_cfg.cache.gyro_fsr);
            mpu_get_accel_fsr(&st->chip_cfg.cache.accel_fsr);
            mpu_get_lpf(&st->chip_cfg.cache.lpf);
            mpu_get_sample_rate(&st->chip_cfg.cache.sample_rate);
            st->chip_cfg.cache.sensors_on = st->chip_cfg.sensors;
            mpu_get_fifo_config(&st->chip_cfg.cache.fifo_sensors);
        }

    } else {
        /* Don't "restore" the previous state if no state has been saved. */
        int ii;
        char *cache_ptr = (char*)&st->chip_cfg.cache;
        for (ii = 0; ii < sizeof(st->chip_cfg.cache); ii++) {
            if (cache_ptr[ii] != 0)
                goto lp_int_restore;
        }
        /* If we reach this point, motion interrupt mode hasn't been used yet. */
        return -1;
    }
lp_int_restore:
    /* Set to invalid values to ensure no I2C writes are skipped. */
    st->chip_cfg.gyro_fsr = 0xFF;
    st->chip_cfg.accel_fsr = 0xFF;
    st->chip_cfg.lpf = 0xFF;
    st->chip_cfg.sample_rate = 0xFFFF;
    st->chip_cfg.sensors = 0xFF;
    st->chip_cfg.fifo_enable = 0xFF;
    st->chip_cfg.clk_src = INV_CLK_PLL;
    mpu_set_sensors(st->chip_cfg.cache.sensors_on);
    mpu_set_gyro_fsr(st->chip_cfg.cache.gyro_fsr);
    mpu_set_accel_fsr(st->chip_cfg.cache.accel_fsr);
    mpu_set_lpf(st->chip_cfg.cache.lpf);
    mpu_set_sample_rate(st->chip_cfg.cache.sample_rate);
    mpu_configure_fifo(st->chip_cfg.cache.fifo_sensors);

    if (st->chip_cfg.cache.dmp_on)
        mpu_set_dmp_state(1);

    st->chip_cfg.int_motion_only = 0;
    return 0;
}
