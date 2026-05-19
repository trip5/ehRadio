#ifndef dsp_full_loc
#define dsp_full_loc
#include <pgmspace.h>
/*************************************************************************************
    HOWTO:
    Copy this file to locale/displayL10n_custom.h
    and modify it
*************************************************************************************/
// Language: Bosnian-Montenegrin
// IETF BCP 47: "bs-ME"
const char mon[] PROGMEM = "Po";
const char tue[] PROGMEM = "Ut";
const char wed[] PROGMEM = "Sr";
const char thu[] PROGMEM = "Če";
const char fri[] PROGMEM = "Pe";
const char sat[] PROGMEM = "Su";
const char sun[] PROGMEM = "Ne";

const char monf[] PROGMEM = "Ponedjeljak";
const char tuef[] PROGMEM = "Utorak";
const char wedf[] PROGMEM = "Srijeda";
const char thuf[] PROGMEM = "Četvrtak";
const char frif[] PROGMEM = "Petak";
const char satf[] PROGMEM = "Subota";
const char sunf[] PROGMEM = "Nedjelja";

const char jan[] PROGMEM = "Januar";
const char feb[] PROGMEM = "Februar";
const char mar[] PROGMEM = "Mart";
const char apr[] PROGMEM = "April";
const char may[] PROGMEM = "Maj";
const char jun[] PROGMEM = "Juni";
const char jul[] PROGMEM = "Juli";
const char aug[] PROGMEM = "Avgust";
const char sep[] PROGMEM = "Septembar";
const char oct[] PROGMEM = "Oktobar";
const char nov[] PROGMEM = "Novembar";
const char dec[] PROGMEM = "Decembar";

const char wn_N[]      PROGMEM = "SJEVER";
const char wn_NNE[]    PROGMEM = "SJ-I";
const char wn_NE[]     PROGMEM = "SI";
const char wn_ENE[]    PROGMEM = "I-SI";
const char wn_E[]      PROGMEM = "ISTOK";
const char wn_ESE[]    PROGMEM = "I-JI";
const char wn_SE[]     PROGMEM = "JI";
const char wn_SSE[]    PROGMEM = "J-JI";
const char wn_S[]      PROGMEM = "JUG";
const char wn_SSW[]    PROGMEM = "J-JZ";
const char wn_SW[]     PROGMEM = "JZ";
const char wn_WSW[]    PROGMEM = "Z-JZ";
const char wn_W[]      PROGMEM = "ZAPAD";
const char wn_WNW[]    PROGMEM = "Z-SZ";
const char wn_NW[]     PROGMEM = "SZ";
const char wn_NNW[]    PROGMEM = "SJ-SZ";

const char* const dow[]     PROGMEM = { sun, mon, tue, wed, thu, fri, sat };
const char* const dowf[]    PROGMEM = { sunf, monf, tuef, wedf, thuf, frif, satf };
const char* const mnths[]   PROGMEM = { jan, feb, mar, apr, may, jun, jul, aug, sep, oct, nov, dec };
const char* const wind[]    PROGMEM = { wn_N, wn_NNE, wn_NE, wn_ENE, wn_E, wn_ESE, wn_SE, wn_SSE, wn_S, wn_SSW, wn_SW, wn_WSW, wn_W, wn_WNW, wn_NW, wn_NNW, wn_N };

const char    const_PlReady[]    PROGMEM = "[ready]";
const char  const_PlStopped[]    PROGMEM = "[stopped]";
const char  const_PlConnect[]    PROGMEM = "[connecting]";
const char  const_DlgVolume[]    PROGMEM = "VOLUME";
const char    const_DlgLost[]    PROGMEM = "* LOST *";
const char  const_DlgUpdate[]    PROGMEM = "* UPDATING *";
const char const_DlgNextion[]    PROGMEM = "NEXTION";
const char  const_waitForSD[]    PROGMEM = "INDEX SD";

const char        apNameTxt[]    PROGMEM = "AP NAME";
#ifdef AP_PASSWORD
  const char        apPassTxt[]    PROGMEM = "PASSWORD";
#else
  const char        apPassTxt[]    PROGMEM = "NO PASSWORD";
#endif

const char       bootstrFmt[]    PROGMEM = "Wi-fi: %s";
const char        apSettFmt[]    PROGMEM = "POVEŽI I OTVORI HTTP://%s/";

#ifdef UPDATEURL
  const char      updFirmware[]    PROGMEM = "Ažuriranje firmvera";
  const char         updFiles[]    PROGMEM = "Ažuriranje datoteka";
  const char        updFailed[]    PROGMEM = "Ažuriranje nije uspjelo";
#endif

const char weather_feelslike[]  PROGMEM = "kao da:";
const char weather_pressure[]   PROGMEM = "pritisak:";
const char weather_humidity[]   PROGMEM = "vlažnost:";
const char weather_wind[]       PROGMEM = "vjetar:";
const char weather_loading[]    PROGMEM = "Preuzimanje trenutnih vremenskih informacija...";

// WMO Weather Code Translations (for Open-Meteo)
const char w_clear_sky[]         PROGMEM = "Bistro nebo";
const char w_overcast[]          PROGMEM = "Oblačno";
const char w_foggy[]             PROGMEM = "Magla";
const char w_drizzle[]           PROGMEM = "Pospij";
const char w_freezing_drizzle[]  PROGMEM = "Zaleđujuća rosulja";
const char w_rain[]              PROGMEM = "Kiš";
const char w_freezing_rain[]     PROGMEM = "Ledena kiša";
const char w_snow[]              PROGMEM = "Snijeg";
const char w_snow_grains[]       PROGMEM = "Snježne zrnce";
const char w_rain_showers[]      PROGMEM = "Kišni pljuskovi";
const char w_snow_showers[]      PROGMEM = "Snježne padavine";
const char w_thunderstorm[]      PROGMEM = "Grmljavinska oluja";
const char w_thunderstorm_hail[] PROGMEM = "Oluja s grmljavinom i gradom";

#endif

