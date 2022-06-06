/*! Конфигурация gps модуля, сгенерированная из конфигурации в
    формате ucenter.
    Файл сгенерирован автоматически, поэтому менять его не рекомендуется,
    так как все изменения могут быть затерты перегенерацией */

#include <stdint.h>

/*! Статические переменные для отдельных сообщений.
    Эти массивы содержат сообщение полностью, без синхрослова в начале
    и контрольной суммы в конце.
    Однако такие поля как cls_id, msg_id, len в сообщениях содержаться */

// Сброс всех настроек кроме
static const uint8_t cfg_reset_config[] = {
		0x06, 0x09, // clsid, msgid
		0x0D, 0x00, // len
		0xFF, 0xFB, 0x00, 0x00, // clear mask
		0x00, 0x00, 0x00, 0x00, // save mask
		0xFF, 0xFF, 0x00, 0x00, // load mask
		0x03 // device mask
};

// Это дефолтная конфигурация антенны, не будет лишним задать её еще разок
static const uint8_t cfg_ant_0[] = {
		0x06, 0x13, // msgid, clsid
		0x04, 0x00, // len
		0x1B, 0x00, // flags
		0x8B, 0x00  // pins
};

// Это настройки разрешенных сообщений.
// По итогу тут включается только UBX-NAX-SOL UBX-TIM-TP и UBX-GPS-TIME
static const uint8_t cfg_msg_7[] = {0x06, 0x01, 0x08, 0x00, 0x0B, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t cfg_msg_8[] = {0x06, 0x01, 0x08, 0x00, 0x0B, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t cfg_msg_9[] = {0x06, 0x01, 0x08, 0x00, 0x0B, 0x33, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t cfg_msg_10[] = {0x06, 0x01, 0x08, 0x00, 0x0B, 0x31, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t cfg_msg_11[] = {0x06, 0x01, 0x08, 0x00, 0x0B, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t cfg_msg_12[] = {0x06, 0x01, 0x08, 0x00, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t cfg_msg_13[] = {0x06, 0x01, 0x08, 0x00, 0x0A, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t cfg_msg_14[] = {0x06, 0x01, 0x08, 0x00, 0x0A, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t cfg_msg_15[] = {0x06, 0x01, 0x08, 0x00, 0x0A, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t cfg_msg_16[] = {0x06, 0x01, 0x08, 0x00, 0x0A, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t cfg_msg_17[] = {0x06, 0x01, 0x08, 0x00, 0x0A, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t cfg_msg_18[] = {0x06, 0x01, 0x08, 0x00, 0x0A, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t cfg_msg_19[] = {0x06, 0x01, 0x08, 0x00, 0x0A, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t cfg_msg_20[] = {0x06, 0x01, 0x08, 0x00, 0x01, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t cfg_msg_21[] = {0x06, 0x01, 0x08, 0x00, 0x01, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t cfg_msg_22[] = {0x06, 0x01, 0x08, 0x00, 0x01, 0x31, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t cfg_msg_23[] = {0x06, 0x01, 0x08, 0x00, 0x01, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t cfg_msg_24[] = {0x06, 0x01, 0x08, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t cfg_msg_25[] = {0x06, 0x01, 0x08, 0x00, 0x01, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t cfg_msg_26[] = {0x06, 0x01, 0x08, 0x00, 0x01, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t cfg_msg_27[] = {0x06, 0x01, 0x08, 0x00, 0x01, 0x06, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00};
static const uint8_t cfg_msg_28[] = {0x06, 0x01, 0x08, 0x00, 0x01, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t cfg_msg_29[] = {0x06, 0x01, 0x08, 0x00, 0x01, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t cfg_msg_30[] = {0x06, 0x01, 0x08, 0x00, 0x01, 0x20, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00};
static const uint8_t cfg_msg_31[] = {0x06, 0x01, 0x08, 0x00, 0x01, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t cfg_msg_32[] = {0x06, 0x01, 0x08, 0x00, 0x01, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t cfg_msg_33[] = {0x06, 0x01, 0x08, 0x00, 0x01, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t cfg_msg_34[] = {0x06, 0x01, 0x08, 0x00, 0x02, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t cfg_msg_35[] = {0x06, 0x01, 0x08, 0x00, 0x0D, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t cfg_msg_36[] = {0x06, 0x01, 0x08, 0x00, 0x0D, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00};
static const uint8_t cfg_msg_37[] = {0x06, 0x01, 0x08, 0x00, 0x0D, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t cfg_msg_38[] = {0x06, 0x01, 0x08, 0x00, 0xF0, 0x00, 0x01, 0x00, 0x01, 0x01, 0x01, 0x01};
static const uint8_t cfg_msg_39[] = {0x06, 0x01, 0x08, 0x00, 0xF0, 0x01, 0x01, 0x00, 0x01, 0x01, 0x01, 0x01};
static const uint8_t cfg_msg_40[] = {0x06, 0x01, 0x08, 0x00, 0xF0, 0x02, 0x01, 0x00, 0x01, 0x01, 0x01, 0x01};
static const uint8_t cfg_msg_41[] = {0x06, 0x01, 0x08, 0x00, 0xF0, 0x03, 0x01, 0x00, 0x01, 0x01, 0x01, 0x01};
static const uint8_t cfg_msg_42[] = {0x06, 0x01, 0x08, 0x00, 0xF0, 0x04, 0x01, 0x00, 0x01, 0x01, 0x01, 0x01};
static const uint8_t cfg_msg_43[] = {0x06, 0x01, 0x08, 0x00, 0xF0, 0x05, 0x01, 0x00, 0x01, 0x01, 0x01, 0x01};
static const uint8_t cfg_msg_44[] = {0x06, 0x01, 0x08, 0x00, 0xF0, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t cfg_msg_45[] = {0x06, 0x01, 0x08, 0x00, 0xF0, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t cfg_msg_46[] = {0x06, 0x01, 0x08, 0x00, 0xF0, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t cfg_msg_47[] = {0x06, 0x01, 0x08, 0x00, 0xF0, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t cfg_msg_48[] = {0x06, 0x01, 0x08, 0x00, 0xF0, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t cfg_msg_49[] = {0x06, 0x01, 0x08, 0x00, 0xF1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t cfg_msg_50[] = {0x06, 0x01, 0x08, 0x00, 0xF1, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t cfg_msg_51[] = {0x06, 0x01, 0x08, 0x00, 0xF1, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};


static const uint8_t cfg_nav5_52[] = {
		0x06, 0x24, // msgid, clsid
		0x24, 0x00, // len
		0xFF, 0xFF, // mask
		0x06, // dyn model (важно! нужен airborn. Это 0x06 (<1g), 0x07 (<2g), 0x08 (<3g))
		0x03, // fixmode
		0x00, 0x00, 0x00, 0x00, // fixedAlt
		0x10, 0x27, 0x00, 0x00, // fixedAltVar
		0x05, // minElev
		0x00, // drLimit
		0xFA, 0x00, // pDop
		0xFA, 0x00, // tDop
		0x64, 0x00, // pAcc
		0x2C, 0x01, // tAcc
		0x00, // staticHoldThresh
		0x3C, // dgpsTimeOut
		0x00, 0x00, 0x00, 0x00, // reserved2
		0x00, 0x00, 0x00, 0x00, // reserved3
		0x00, 0x00, 0x00, 0x00  // reserved4
};

// Управление питанием
static const uint8_t cfg_pm2_56[] = {
		0x06, 0x3B, // clsid, msgid
		0x2C, 0x00, // длина
		0x01, // version
		0x06, 0x00, 0x00, // reserved1, reserved2, reserved3
		0x0E, 0x98, 0x03, 0x00, // flags: из важных:
								// cyclic tracking, update eph, update rtc, doNotEnterOff
		0xE8, 0x03, 0x00, 0x00, // update period (1000 ms)
		0x10, 0x27, 0x00, 0x00, // search period (10 000 ms) u-center говорит что это не меняется
		0x00, 0x00, 0x00, 0x00, // grid offset - используется только в on/off режиме
		0xFF, 0xFF, // on time - время в состоянии TRACKING в которое входим сразу после фикса
					// по завершению TRACKING переход в Power Optimized Tracking
					// Дефолтное значение 0x02, 0x00. Попробуем замаксить активный трекинг
		0xFF, 0xFF, // min acq time - минимальное время в режиме поиска спутников
					// (ACQUSITION) (которое перед TRACKING).
					// Дефолтное значение 0x00 0x00. Попробуем замаксить.
		0x2C, 0x01, // reserved 4
		0x00, 0x00, // reserved 5
		0x4F, 0xC1, 0x03, 0x00, // reserved 6
		0x86, 0x02, 0x00, 0x00, // reserved 7
		0xFE, // reserved 8
		0x00, // reserved 9
		0x00, 0x00, // reserved 10
		0x64, 0x40, 0x01, 0x00 // reserved 11
};

// Настройки скоростей замеров и базового времени. Настраиваем время
// остальное по дефолту
static const uint8_t cfg_rate_62[] = {
		0x06, 0x08, // msgid clsid
		0x06, 0x00, // len
		0xE8, 0x03, // meas rate (ms)
		0x01, 0x00, // nav rate (Hz)
		0x01, 0x00  // time ref (0 - UTC, 1 - GPS, 2 - GLO, и там много еще)
};

// Поставим power mode на "ничего не экономить"
static const uint8_t cfg_rxm_64[] = {
		0x06, 0x11, // clsid, msgid
		0x02, 0x00, // len
		0x08, // reserved
		0x00  // power mode
};

// Настраиваем PPS сигнал
static const uint8_t cfg_tp_66[] = {
		0x06, 0x07, // msgid, clsid
		0x14, 0x00, // len
		0x40, 0x42, 0x0F, 0x00, // interval (1 000 000 us)
		0xa0, 0x86, 0x01, 0x00, // pulselen (100 000 us)
		0x01, // status (+1 positive, 0 disabled, -1 negative)
		0x01, // timeref 1 gps time
		0x01, // flags (0x01 == включен всегда, даже если нет хорошего времени, 0x00 - pps только с фиксом)
		0x00, // reserved1
		0x32, 0x00, // antenna cable delay (50 ns)
		0x00, 0x00, // reGroupDelay (0 ns)
		0x00, 0x00, 0x00, 0x00 // user delay (0 ns)
};

/*! Эта переменная не статическая и предполагается её использование в
    прочих файлах проекта. Эта переменная является этаким
    "двумерным массивом" конфигурационных сообщений.

    Конец массива терменирован нулевым указателем */
const uint8_t * ublox_neo6_cfg_msgs[] = {
		cfg_reset_config, // начинаем с резета конфигурации
		cfg_ant_0,
		cfg_msg_7, cfg_msg_8, cfg_msg_9, cfg_msg_10, cfg_msg_11, cfg_msg_12, cfg_msg_13, cfg_msg_14, cfg_msg_15,
		cfg_msg_16, cfg_msg_17, cfg_msg_18, cfg_msg_19, cfg_msg_20, cfg_msg_21, cfg_msg_22, cfg_msg_23, cfg_msg_24,
		cfg_msg_25, cfg_msg_26, cfg_msg_27, cfg_msg_28, cfg_msg_29, cfg_msg_30, cfg_msg_31, cfg_msg_32, cfg_msg_33,
		cfg_msg_34, cfg_msg_35, cfg_msg_36, cfg_msg_37, cfg_msg_38, cfg_msg_39, cfg_msg_40, cfg_msg_41, cfg_msg_42,
		cfg_msg_43, cfg_msg_44, cfg_msg_45, cfg_msg_46, cfg_msg_47, cfg_msg_48, cfg_msg_49, cfg_msg_50, cfg_msg_51,
		cfg_nav5_52,
		cfg_pm2_56,
		cfg_rate_62,
		cfg_rxm_64,
		cfg_tp_66,
		0
};
