#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <math.h>
#include <time.h>
#include <dlfcn.h>
#include <semaphore.h>
#include <signal.h>
#include <termios.h>
#define  LOG_TAG  "gps_nowplus"
#include <cutils/log.h>
#include <cutils/sockets.h>
#include <hardware_legacy/gps.h>

/* the name of the qemud-controlled socket */
#define  GPS_DEVICE  "/dev/ttyGPS0"
#define  GPS_DEBUG  1

#if GPS_DEBUG
#  define  D(...)   LOGD(__VA_ARGS__)
#else
#  define  D(...)   ((void)0)
#endif

#define GPS_STATUS_CB(_cb, _s)    \
  if ((_cb).status_cb) {          \
    GpsStatus gps_status;         \
    gps_status.status = (_s);     \
    (_cb).status_cb(&gps_status); \
    D("gps status callback: 0x%x", _s); \
  }
  
/* Nmea Parser stuff */
#define  NMEA_MAX_SIZE  83

enum {
  STATE_QUIT  = 0,
  STATE_INIT  = 1,
  STATE_START = 2
};

typedef struct {
    int     pos;
    int     overflow;
    int     utc_year;
    int     utc_mon;
    int     utc_day;
    int     utc_diff;
    GpsLocation  fix;
    GpsSvStatus  sv_status;
    int     sv_status_changed;
    char    in[ NMEA_MAX_SIZE+1 ];
} NmeaReader;

/* Since NMEA parser requires lcoks */
#define GPS_STATE_LOCK_FIX(_s)         \
{                                      \
  int ret;                             \
  do {                                 \
    ret = sem_wait(&(_s)->fix_sem);    \
  } while (ret < 0 && errno == EINTR);   \
}
#define GPS_STATE_UNLOCK_FIX(_s)       \
  sem_post(&(_s)->fix_sem)
  
typedef struct {
    int                     init;
    int                     fd;
    GpsCallbacks            callbacks;
    pthread_t               thread;
    pthread_t               tmr_thread;
    int                     control[2];
    int                     fix_freq;
    sem_t                   fix_sem;
    int                     first_fix;
    NmeaReader              reader;
} GpsState;

static GpsState  _gps_state[1];
static GpsState *gps_state = _gps_state;
static void *gps_timer_thread( void*  arg );

/*****************************************************************/
/*****************************************************************/
/*****                                                       *****/
/*****       N M E A   T O K E N I Z E R                     *****/
/*****                                                       *****/
/*****************************************************************/
/*****************************************************************/
typedef struct {
    const char*  p;
    const char*  end;
} Token;

#define  MAX_NMEA_TOKENS  32

typedef struct {
    int     count;
    Token   tokens[ MAX_NMEA_TOKENS ];
} NmeaTokenizer;

static int
nmea_tokenizer_init( NmeaTokenizer*  t, const char*  p, const char*  end )
{
    int    count = 0;
    char*  q;
    // the initial '$' is optional
    if (p < end && p[0] == '$')
        p += 1;
    // remove trailing newline
    if (end > p && end[-1] == '\n') {
        end -= 1;
        if (end > p && end[-1] == '\r')
            end -= 1;
    }
    // get rid of checksum at the end of the sentecne
    if (end >= p+3 && end[-3] == '*') {
        end -= 3;
    }
    while (p < end) {
        const char*  q = p;
        q = memchr(p, ',', end-p);
        if (q == NULL)
            q = end;
        if (count < MAX_NMEA_TOKENS) {
            t->tokens[count].p   = p;
            t->tokens[count].end = q;
            count += 1;
        }
        if (q < end)
            q += 1;
        p = q;
    }
    t->count = count;
    return count;
}
static Token
nmea_tokenizer_get( NmeaTokenizer*  t, int  index )
{
    Token  tok;
    static const char*  dummy = "";
    if (index < 0 || index >= t->count) {
        tok.p = tok.end = dummy;
    } else
        tok = t->tokens[index];
    return tok;
}
static int
str2int( const char*  p, const char*  end )
{
    int   result = 0;
    int   len    = end - p;
    if (len == 0) {
      return -1;
    }
    for ( ; len > 0; len--, p++ )
    {
        int  c;
        if (p >= end)
            goto Fail;
        c = *p - '0';
        if ((unsigned)c >= 10)
            goto Fail;
        result = result*10 + c;
    }
    return  result;
Fail:
    return -1;
}
static double
str2float( const char*  p, const char*  end )
{
    int   result = 0;
    int   len    = end - p;
    char  temp[16];
    if (len == 0) {
      return -1.0;
    }
    if (len >= (int)sizeof(temp))
        return 0.;
    memcpy( temp, p, len );
    temp[len] = 0;
    return strtod( temp, NULL );
}
/** @desc Convert struct tm to time_t (time zone neutral).
 *
 * The one missing function in libc: It works basically like mktime, with the main difference that
 * it does no time zone-related processing but interprets the members of the struct tm as UTC.
 * Unlike mktime, it will not modify any fields of the tm structure; if you need this behavior, call
 * mktime before this function.
 *
 * @param t Pointer to a struct tm containing date and time. Only the tm_year, tm_mon, tm_mday,
 * tm_hour, tm_min and tm_sec members will be evaluated, all others will be ignored.
 *
 * @return The epoch time (seconds since 1970-01-01 00:00:00 UTC) which corresponds to t.
 *
 * @author Originally written by Philippe De Muyter <phdm@macqel.be> for Lynx.
 * http://lynx.isc.org/current/lynx2-8-8/src/mktime.c
 */
static time_t
mkgmtime(struct tm *t)
{
    short month, year;
    time_t result;
    static int m_to_d[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    month = t->tm_mon;
    year = t->tm_year + month / 12 + 1900;
    month %= 12;
    if (month < 0) {
	    year -= 1;
	    month += 12;
    }
    result = (year - 1970) * 365 + m_to_d[month];
    if (month <= 1)
	    year -= 1;
    result += (year - 1968) / 4;
    result -= (year - 1900) / 100;
    result += (year - 1600) / 400;
    result += t->tm_mday;
    result -= 1;
    result *= 24;
    result += t->tm_hour;
    result *= 60;
    result += t->tm_min;
    result *= 60;
    result += t->tm_sec;
    return (result);
}
/*****************************************************************/
/*****************************************************************/
/*****                                                       *****/
/*****       N M E A   P A R S E R                           *****/
/*****                                                       *****/
/*****************************************************************/
/*****************************************************************/
static void
nmea_reader_update_utc_diff( NmeaReader*  r )
{
    time_t         now = time(NULL);
    struct tm      tm_local;
    struct tm      tm_utc;
    long           time_local, time_utc;
    gmtime_r( &now, &tm_utc );
    localtime_r( &now, &tm_local );
    time_local = mktime(&tm_local);
    time_utc = mktime(&tm_utc);
    r->utc_diff = time_local - time_utc;
}
static void
nmea_reader_init( NmeaReader*  r )
{
    memset( r, 0, sizeof(*r) );
    r->pos      = 0;
    r->overflow = 0;
    r->utc_year = -1;
    r->utc_mon  = -1;
    r->utc_day  = -1;
    // not sure if we still need this (this module doesn't use utc_diff)
    nmea_reader_update_utc_diff( r );
}
static int
nmea_reader_update_time( NmeaReader*  r, Token  tok )
{
    int        hour, minute, seconds, milliseconds;
    struct tm  tm;
    time_t     fix_time;
    if (tok.p + 6 > tok.end)
        return -1;
    if (r->utc_year < 0) {
        // no date, can't return valid timestamp (never ever make up a date, this could wreak havoc)
        return -1;
    }
    else
    {
        tm.tm_year = r->utc_year - 1900;
        tm.tm_mon  = r->utc_mon - 1;
        tm.tm_mday = r->utc_day;
    }
    hour    = str2int(tok.p,   tok.p+2);
    minute  = str2int(tok.p+2, tok.p+4);
    seconds = str2int(tok.p+4, tok.p+6);
    // parse also milliseconds (if present) for better precision
    milliseconds = 0;
    if (tok.end - (tok.p+7) == 2) {
        milliseconds = str2int(tok.p+7, tok.end) * 10;
    }
    else if (tok.end - (tok.p+7) == 1) {
        milliseconds = str2int(tok.p+7, tok.end) * 100;
    }
    else if (tok.end - (tok.p+7) >= 3) {
        milliseconds = str2int(tok.p+7, tok.p+10);
    }
    // the following is only guaranteed to work if we have previously set a correct date, so be sure
    // to always do that before
    tm.tm_hour = hour;
    tm.tm_min  = minute;
    tm.tm_sec  = seconds;
    fix_time = mkgmtime( &tm );
    r->fix.timestamp = (long long)fix_time * 1000 + milliseconds;
    return 0;
}
static int
nmea_reader_update_cdate( NmeaReader*  r, Token  tok_d, Token tok_m, Token tok_y )
{
    if ( (tok_d.p + 2 > tok_d.end) ||
         (tok_m.p + 2 > tok_m.end) ||
         (tok_y.p + 4 > tok_y.end) )
        return -1;
    r->utc_day = str2int(tok_d.p,   tok_d.p+2);
    r->utc_mon = str2int(tok_m.p, tok_m.p+2);
    r->utc_year = str2int(tok_y.p, tok_y.p+4);
    return 0;
}
static int
nmea_reader_update_date( NmeaReader*  r, Token  date, Token  time )
{
    Token  tok = date;
    int    day, mon, year;
    if (tok.p + 6 != tok.end) {
        D("date not properly formatted: '%.*s'", tok.end-tok.p, tok.p);
        return -1;
    }
    day  = str2int(tok.p, tok.p+2);
    mon  = str2int(tok.p+2, tok.p+4);
    year = str2int(tok.p+4, tok.p+6) + 2000;
    if ((day|mon|year) < 0) {
        D("date not properly formatted: '%.*s'", tok.end-tok.p, tok.p);
        return -1;
    }
    r->utc_year  = year;
    r->utc_mon   = mon;
    r->utc_day   = day;
    return nmea_reader_update_time( r, time );
}
static double
convert_from_hhmm( Token  tok )
{
    double  val     = str2float(tok.p, tok.end);
    int     degrees = (int)(floor(val) / 100);
    double  minutes = val - degrees*100.;
    double  dcoord  = degrees + minutes / 60.0;
    return dcoord;
}
static int
nmea_reader_update_latlong( NmeaReader*  r,
                            Token        latitude,
                            char         latitudeHemi,
                            Token        longitude,
                            char         longitudeHemi )
{
    double   lat, lon;
    Token    tok;
    tok = latitude;
    if (tok.p + 6 > tok.end) {
        D("latitude is too short: '%.*s'", tok.end-tok.p, tok.p);
        return -1;
    }
    lat = convert_from_hhmm(tok);
    if (latitudeHemi == 'S')
        lat = -lat;
    tok = longitude;
    if (tok.p + 6 > tok.end) {
        D("longitude is too short: '%.*s'", tok.end-tok.p, tok.p);
        return -1;
    }
    lon = convert_from_hhmm(tok);
    if (longitudeHemi == 'W')
        lon = -lon;
    r->fix.flags    |= GPS_LOCATION_HAS_LAT_LONG;
    r->fix.latitude  = lat;
    r->fix.longitude = lon;
    return 0;
}
static int
nmea_reader_update_altitude( NmeaReader*  r,
                             Token        altitude,
                             Token        units )
{
    double  alt;
    Token   tok = altitude;
    if (tok.p >= tok.end)
        return -1;
    r->fix.flags   |= GPS_LOCATION_HAS_ALTITUDE;
    r->fix.altitude = str2float(tok.p, tok.end);
    return 0;
}
static int
nmea_reader_update_accuracy( NmeaReader*  r,
                             Token        accuracy )
{
    double  acc;
    Token   tok = accuracy;
    if (tok.p >= tok.end)
        return -1;
    r->fix.accuracy = str2float(tok.p, tok.end);
    if (r->fix.accuracy == 99.99){
      return 0;
    }
    r->fix.flags   |= GPS_LOCATION_HAS_ACCURACY;
    return 0;
}
static int
nmea_reader_update_bearing( NmeaReader*  r,
                            Token        bearing )
{
    double  alt;
    Token   tok = bearing;
    if (tok.p >= tok.end)
        return -1;
    r->fix.flags   |= GPS_LOCATION_HAS_BEARING;
    r->fix.bearing  = str2float(tok.p, tok.end);
    return 0;
}
static int
nmea_reader_update_speed( NmeaReader*  r,
                          Token        speed )
{
    double  alt;
    Token   tok = speed;
    if (tok.p >= tok.end)
        return -1;
    r->fix.flags   |= GPS_LOCATION_HAS_SPEED;
    // convert knots into m/sec (1 knot equals 1.852 km/h, 1 km/h equals 3.6 m/s)
    // since 1.852 / 3.6 is an odd value (periodic), we're calculating the quotient on the fly
    // to obtain maximum precision (we don't want 1.9999 instead of 2)
    r->fix.speed    = str2float(tok.p, tok.end) * 1.852 / 3.6;
    return 0;
}
static void
nmea_reader_parse( NmeaReader*  r )
{
   /* we received a complete sentence, now parse it to generate
    * a new GPS fix...
    */
    NmeaTokenizer  tzer[1];
    Token          tok;
    D("Received: '%.*s'", r->pos, r->in);
    if (r->pos < 9) {
        D("Too short. discarded.");
        return;
    }
    nmea_tokenizer_init(tzer, r->in, r->in + r->pos);
#if GPS_DEBUG
    {
        int  n;
        D("Found %d tokens", tzer->count);
        for (n = 0; n < tzer->count; n++) {
            Token  tok = nmea_tokenizer_get(tzer,n);
            D("%2d: '%.*s'", n, tok.end-tok.p, tok.p);
        }
    }
#endif
    tok = nmea_tokenizer_get(tzer, 0);
    if (tok.p + 5 > tok.end) {
        D("sentence id '%.*s' too short, ignored.", tok.end-tok.p, tok.p);
        return;
    }
    // ignore first two characters.
    tok.p += 2;
    if ( !memcmp(tok.p, "GGA", 3) ) {
        // GPS fix
        Token  tok_fixstaus      = nmea_tokenizer_get(tzer,6);
        if ((tok_fixstaus.p[0] > '0') && (r->utc_year >= 0)) {
          // ignore this until we have a valid timestamp
          Token  tok_time          = nmea_tokenizer_get(tzer,1);
          Token  tok_latitude      = nmea_tokenizer_get(tzer,2);
          Token  tok_latitudeHemi  = nmea_tokenizer_get(tzer,3);
          Token  tok_longitude     = nmea_tokenizer_get(tzer,4);
          Token  tok_longitudeHemi = nmea_tokenizer_get(tzer,5);
          Token  tok_altitude      = nmea_tokenizer_get(tzer,9);
          Token  tok_altitudeUnits = nmea_tokenizer_get(tzer,10);
          // don't use this as we have no fractional seconds and no date; there are better ways to
          // get a good timestamp from GPS
          //nmea_reader_update_time(r, tok_time);
          nmea_reader_update_latlong(r, tok_latitude,
                                        tok_latitudeHemi.p[0],
                                        tok_longitude,
                                        tok_longitudeHemi.p[0]);
          nmea_reader_update_altitude(r, tok_altitude, tok_altitudeUnits);
        }
    } else if ( !memcmp(tok.p, "GLL", 3) ) {
        Token  tok_fixstaus      = nmea_tokenizer_get(tzer,6);
        if ((tok_fixstaus.p[0] == 'A') && (r->utc_year >= 0)) {
          // ignore this until we have a valid timestamp
          Token  tok_latitude      = nmea_tokenizer_get(tzer,1);
          Token  tok_latitudeHemi  = nmea_tokenizer_get(tzer,2);
          Token  tok_longitude     = nmea_tokenizer_get(tzer,3);
          Token  tok_longitudeHemi = nmea_tokenizer_get(tzer,4);
          Token  tok_time          = nmea_tokenizer_get(tzer,5);
          // don't use this as we have no fractional seconds and no date; there are better ways to
          // get a good timestamp from GPS
          //nmea_reader_update_time(r, tok_time);
          nmea_reader_update_latlong(r, tok_latitude,
                                        tok_latitudeHemi.p[0],
                                        tok_longitude,
                                        tok_longitudeHemi.p[0]);
        }
    } else if ( !memcmp(tok.p, "GSA", 3) ) {
        Token  tok_fixStatus   = nmea_tokenizer_get(tzer, 2);
        int i;
        if (tok_fixStatus.p[0] != '\0' && tok_fixStatus.p[0] != '1') {
          Token  tok_accuracy      = nmea_tokenizer_get(tzer, 15);
          nmea_reader_update_accuracy(r, tok_accuracy);
          r->sv_status.used_in_fix_mask = 0ul;
          for (i = 3; i <= 14; ++i){
            Token  tok_prn  = nmea_tokenizer_get(tzer, i);
            int prn = str2int(tok_prn.p, tok_prn.end);
            if (prn > 0){
              r->sv_status.used_in_fix_mask |= (1ul << (32 - prn));
              r->sv_status_changed = 1;
              D("%s: fix mask is %d", __FUNCTION__, r->sv_status.used_in_fix_mask);
            }
          }
        }
    } else if ( !memcmp(tok.p, "GSV", 3) ) {
        Token  tok_noSatellites  = nmea_tokenizer_get(tzer, 3);
        int    noSatellites = str2int(tok_noSatellites.p, tok_noSatellites.end);
       
        if (noSatellites > 0) {
          Token  tok_noSentences   = nmea_tokenizer_get(tzer, 1);
          Token  tok_sentence      = nmea_tokenizer_get(tzer, 2);
          int sentence = str2int(tok_sentence.p, tok_sentence.end);
          int totalSentences = str2int(tok_noSentences.p, tok_noSentences.end);
          int curr;
          int i;
          
          if (sentence == 1) {
              r->sv_status_changed = 0;
              r->sv_status.num_svs = 0;
          }
          curr = r->sv_status.num_svs;
          i = 0;
          while (i < 4 && r->sv_status.num_svs < noSatellites){
                 Token  tok_prn = nmea_tokenizer_get(tzer, i * 4 + 4);
                 Token  tok_elevation = nmea_tokenizer_get(tzer, i * 4 + 5);
                 Token  tok_azimuth = nmea_tokenizer_get(tzer, i * 4 + 6);
                 Token  tok_snr = nmea_tokenizer_get(tzer, i * 4 + 7);
                 r->sv_status.sv_list[curr].prn = str2int(tok_prn.p, tok_prn.end);
                 r->sv_status.sv_list[curr].elevation = str2float(tok_elevation.p, tok_elevation.end);
                 r->sv_status.sv_list[curr].azimuth = str2float(tok_azimuth.p, tok_azimuth.end);
                 r->sv_status.sv_list[curr].snr = str2float(tok_snr.p, tok_snr.end);
                 r->sv_status.num_svs += 1;
                 curr += 1;
                 i += 1;
          }
          if (sentence == totalSentences) {
              r->sv_status_changed = 1;
          }
          D("%s: GSV message with total satellites %d", __FUNCTION__, noSatellites);   
        }
    } else if ( !memcmp(tok.p, "RMC", 3) ) {
        Token  tok_fixStatus     = nmea_tokenizer_get(tzer,2);
        if (tok_fixStatus.p[0] == 'A')
        {
          Token  tok_time          = nmea_tokenizer_get(tzer,1);
          Token  tok_latitude      = nmea_tokenizer_get(tzer,3);
          Token  tok_latitudeHemi  = nmea_tokenizer_get(tzer,4);
          Token  tok_longitude     = nmea_tokenizer_get(tzer,5);
          Token  tok_longitudeHemi = nmea_tokenizer_get(tzer,6);
          Token  tok_speed         = nmea_tokenizer_get(tzer,7);
          Token  tok_bearing       = nmea_tokenizer_get(tzer,8);
          Token  tok_date          = nmea_tokenizer_get(tzer,9);
            nmea_reader_update_date( r, tok_date, tok_time );
            nmea_reader_update_latlong( r, tok_latitude,
                                           tok_latitudeHemi.p[0],
                                           tok_longitude,
                                           tok_longitudeHemi.p[0] );
            nmea_reader_update_bearing( r, tok_bearing );
            nmea_reader_update_speed  ( r, tok_speed );
        }
    } else if ( !memcmp(tok.p, "VTG", 3) ) {
        Token  tok_fixStatus     = nmea_tokenizer_get(tzer,9);
        if (tok_fixStatus.p[0] != '\0' && tok_fixStatus.p[0] != 'N')
        {
            Token  tok_bearing       = nmea_tokenizer_get(tzer,1);
            Token  tok_speed         = nmea_tokenizer_get(tzer,5);
            nmea_reader_update_bearing( r, tok_bearing );
            nmea_reader_update_speed  ( r, tok_speed );
        }
    } else if ( !memcmp(tok.p, "ZDA", 3) ) {
        Token  tok_time;
        Token  tok_year  = nmea_tokenizer_get(tzer,4);
        tok_time  = nmea_tokenizer_get(tzer,1);
        if ((tok_year.p[0] != '\0') && (tok_time.p[0] != '\0')) {
          // make sure to always set date and time together, lest bad things happen
          Token  tok_day   = nmea_tokenizer_get(tzer,2);
          Token  tok_mon   = nmea_tokenizer_get(tzer,3);
          nmea_reader_update_cdate( r, tok_day, tok_mon, tok_year );
          nmea_reader_update_time(r, tok_time);
        }
    } else {
        tok.p -= 2;
        D("unknown sentence '%.*s", tok.end-tok.p, tok.p);
    }
    if (!gps_state->first_fix &&
        gps_state->init == STATE_INIT &&
        r->fix.flags & GPS_LOCATION_HAS_LAT_LONG) {
        if (gps_state->callbacks.location_cb) {
            gps_state->callbacks.location_cb( &r->fix );
            r->fix.flags = 0;
        }
        gps_state->first_fix = 1;
    }
#if 0
    if (r->fix.flags != 0) {
#if GPS_DEBUG
        char   temp[256];
        char*  p   = temp;
        char*  end = p + sizeof(temp);
        struct tm   utc;
        p += snprintf( p, end-p, "sending fix" );
        if (r->fix.flags & GPS_LOCATION_HAS_LAT_LONG) {
            p += snprintf(p, end-p, " lat=%g lon=%g", r->fix.latitude, r->fix.longitude);
        }
        if (r->fix.flags & GPS_LOCATION_HAS_ALTITUDE) {
            p += snprintf(p, end-p, " altitude=%g", r->fix.altitude);
        }
        if (r->fix.flags & GPS_LOCATION_HAS_SPEED) {
            p += snprintf(p, end-p, " speed=%g", r->fix.speed);
        }
        if (r->fix.flags & GPS_LOCATION_HAS_BEARING) {
            p += snprintf(p, end-p, " bearing=%g", r->fix.bearing);
        }
        if (r->fix.flags & GPS_LOCATION_HAS_ACCURACY) {
            p += snprintf(p,end-p, " accuracy=%g", r->fix.accuracy);
        }
        gmtime_r( (time_t*) &r->fix.timestamp, &utc );
        p += snprintf(p, end-p, " time=%s", asctime( &utc ) );
        D(temp);
#endif
        if (r->callback) {
            r->callback( &r->fix );
            r->fix.flags = 0;
        }
        else {
            D("no callback, keeping data until needed !");
        }
    }
#endif
}
static void
nmea_reader_addc( NmeaReader*  r, int  c )
{
    if (r->overflow) {
        r->overflow = (c != '\n');
        return;
    }
    if (r->pos >= (int) sizeof(r->in)-1 ) {
        r->overflow = 1;
        r->pos      = 0;
        return;
    }
    r->in[r->pos] = (char)c;
    r->pos       += 1;
    if (c == '\n') {
        GPS_STATE_LOCK_FIX(gps_state);
        nmea_reader_parse( r );
        GPS_STATE_UNLOCK_FIX(gps_state);
        r->pos = 0;
    }
}
/*****************************************************************/
/*****************************************************************/
/*****                                                       *****/
/*****       C O N N E C T I O N   S T A T E                 *****/
/*****                                                       *****/
/*****************************************************************/
/*****************************************************************/
static int gps_open_device(const char * device);
/* commands sent to the gps thread */
enum {
    CMD_QUIT  = 0,
    CMD_START = 1,
    CMD_STOP  = 2
};
static void
gps_state_done( GpsState*  s )
{
    // tell the thread to quit, and wait for it
    char   cmd = CMD_QUIT;
    void*  dummy;
    int ret;
    D("gps send quit command");
    do { ret=write( s->control[0], &cmd, 1 ); }
    while (ret < 0 && errno == EINTR);
    D("gps waiting for command thread to stop");
    pthread_join(s->thread, &dummy);
    /* Timer thread depends on this state check */
    s->init = STATE_QUIT;
    s->fix_freq = -1;
    // close the control socket pair
    close( s->control[0] ); s->control[0] = -1;
    close( s->control[1] ); s->control[1] = -1;
    // close connection to the QEMU GPS daemon
    close( s->fd ); s->fd = -1;
    sem_destroy(&s->fix_sem);
    memset(s, 0, sizeof(*s));
    D("gps deinit complete");
}
static void
gps_state_start( GpsState*  s )
{
    char  cmd = CMD_START;
    int   ret;
    do { ret=write( s->control[0], &cmd, 1 ); }
    while (ret < 0 && errno == EINTR);
    if (ret != 1)
        D("%s: could not send CMD_START command: ret=%d: %s",
          __FUNCTION__, ret, strerror(errno));
}
static void
gps_state_stop( GpsState*  s )
{
    char  cmd = CMD_STOP;
    int   ret;
    do { ret=write( s->control[0], &cmd, 1 ); }
    while (ret < 0 && errno == EINTR);
    if (ret != 1)
        D("%s: could not send CMD_STOP command: ret=%d: %s",
          __FUNCTION__, ret, strerror(errno));
}
static int
epoll_register( int  epoll_fd, int  fd )
{
    struct epoll_event  ev;
    int                 ret, flags;
    /* important: make the fd non-blocking */
    flags = fcntl(fd, F_GETFL);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    ev.events  = EPOLLIN;
    ev.data.fd = fd;
    do {
        ret = epoll_ctl( epoll_fd, EPOLL_CTL_ADD, fd, &ev );
    } while (ret < 0 && errno == EINTR);
    return ret;
}
static int
epoll_deregister( int  epoll_fd, int  fd )
{
    int  ret;
    do {
        ret = epoll_ctl( epoll_fd, EPOLL_CTL_DEL, fd, NULL );
    } while (ret < 0 && errno == EINTR);
    return ret;
}
/* this is the main thread, it waits for commands from gps_state_start/stop and,
 * when started, messages from the QEMU GPS daemon. these are simple NMEA sentences
 * that must be parsed to be converted into GPS fixes sent to the framework
 */
static void*
gps_state_thread( void*  arg )
{
    GpsState*   state = (GpsState*) arg;
    NmeaReader  *reader;
    int         epoll_fd   = epoll_create(2);
    int         started    = 0;
    int         gps_fd     = state->fd;
    int         control_fd = state->control[1];
    reader = &state->reader;
    nmea_reader_init( reader );
    // register control file descriptors for polling
    epoll_register( epoll_fd, control_fd );
    epoll_register( epoll_fd, gps_fd );
    D("gps thread running");
    GPS_STATUS_CB(state->callbacks, GPS_STATUS_ENGINE_ON);
    // now loop
    for (;;) {
        struct epoll_event   events[2];
        int                  ne, nevents;
        nevents = epoll_wait( epoll_fd, events, 2, -1 );
        if (nevents < 0) {
            if (errno != EINTR)
                LOGE("epoll_wait() unexpected error: %s", strerror(errno));
            continue;
        }
        D("gps thread received %d events", nevents);
        for (ne = 0; ne < nevents; ne++) {
            if ((events[ne].events & (EPOLLERR|EPOLLHUP)) != 0) {
                LOGE("EPOLLERR or EPOLLHUP after epoll_wait() !?");
                goto Exit;
            }
            if ((events[ne].events & EPOLLIN) != 0) {
                int  fd = events[ne].data.fd;
                if (fd == control_fd)
                {
                    char  cmd = 255;
                    int   ret;
                    D("gps control fd event");
                    do {
                        ret = read( fd, &cmd, 1 );
                    } while (ret < 0 && errno == EINTR);
                    if (cmd == CMD_QUIT) {
                        D("gps thread quitting on demand");
                        goto Exit;
                    }
                    else if (cmd == CMD_START) {
                        if (!started) {
                            D("gps thread starting  location_cb=%p", state->callbacks.location_cb);
                            started = 1;
                            
                            GPS_STATUS_CB(state->callbacks, GPS_STATUS_SESSION_BEGIN);
                            state->init = STATE_START;
                            if ( pthread_create( &state->tmr_thread, NULL, gps_timer_thread, state ) != 0 ) {
                                LOGE("could not create gps timer thread: %s", strerror(errno));
                                started = 0;
                                state->init = STATE_INIT;
                                goto Exit;
                            }
                        }
                    }
                    else if (cmd == CMD_STOP) {
                        if (started) {
                            void *dummy;
                            D("gps thread stopping");
                            started = 0;
                            
                            state->init = STATE_INIT;
                            pthread_join(state->tmr_thread, &dummy);
                            GPS_STATUS_CB(state->callbacks, GPS_STATUS_SESSION_END);
                        }
                    }
                }
                else if (fd == gps_fd)
                {
                    char buf[512];
                    int  nn, ret;
                    do {
                        ret = read( fd, buf, sizeof(buf) );
                    } while (ret < 0 && errno == EINTR);
                    if (ret > 0)
                        for (nn = 0; nn < ret; nn++)
                            nmea_reader_addc( reader, buf[nn] );
                    D("gps fd event end");
                }
                else
                {
                    LOGE("epoll_wait() returned unkown fd %d ?", fd);
                }
            }
        }
    }
Exit:
    GPS_STATUS_CB(state->callbacks, GPS_STATUS_ENGINE_OFF);
    
    
    return NULL;
}
static void*
gps_timer_thread( void*  arg )
{
  GpsState *state = (GpsState *)arg;
  D("gps entered timer thread");
  do {
    D("gps timer exp");
    GPS_STATE_LOCK_FIX(state);
    if (state->reader.fix.flags != 0) {
      D("gps fix cb: 0x%x", state->reader.fix.flags);
      if (state->callbacks.location_cb) {
          state->callbacks.location_cb( &state->reader.fix );
          state->reader.fix.flags = 0;
          state->first_fix = 1;
      }
      if (state->fix_freq == 0) {
        state->fix_freq = -1;
      }
    }
    if (state->reader.sv_status_changed != 0) {
      D("gps sv status callback");
      if (state->callbacks.sv_status_cb) {
          state->callbacks.sv_status_cb( &state->reader.sv_status );
          state->reader.sv_status_changed = 0;
      }
    }
    GPS_STATE_UNLOCK_FIX(state);
//    sleep(state->fix_freq);
    sleep(1);
  } while(state->init == STATE_START);
  D("gps timer thread destroyed");
  return NULL;
}
 static void
gps_state_init( GpsState*  state )
{
    int    ret;
    int    done = 0;
    struct sigevent tmr_event;
    state->init       = STATE_INIT;
    state->control[0] = -1;
    state->control[1] = -1;
    state->fd         = -1;
    state->fix_freq   = 1;
    state->first_fix  = 0;
    if (sem_init(&state->fix_sem, 0, 1) != 0) {
      D("gps semaphore initialization failed! errno = %d", errno);
      return;
    }
    state->fd = gps_open_device(GPS_DEVICE);
    D("gps will read from %s", GPS_DEVICE);
	 
    if ( socketpair( AF_LOCAL, SOCK_STREAM, 0, state->control ) < 0 ) {
        LOGE("could not create thread control socket pair: %s", strerror(errno));
        goto Fail;
    }
    if ( pthread_create( &state->thread, NULL, gps_state_thread, state ) != 0 ) {
        LOGE("could not create gps thread: %s", strerror(errno));
        goto Fail;
    }
    D("gps state initialized");
    return;
Fail:
    gps_state_done( state );
}
static int
gps_open_device(const char * device)
{
	int fd;
	struct termios attr;
	speed_t speed=B4800;
	fd=open(device,O_RDONLY);
	if(fd<=0)
	{
		LOGE("could not open gps device %s", device);
		return -1;
	}
	
    if (tcgetattr(fd, &attr) < 0)
    {
        LOGE("error getting serial port settings!");
		close(fd); 
        return -1;
    }
    attr.c_cflag &= ~PARENB;  // no parity
    attr.c_cflag &= ~CSIZE;   // 8-bit bytes (first, clear mask, 
    attr.c_cflag |= CS8;      //              then, set value)
    cfmakeraw(&attr);
    attr.c_cflag |= CLOCAL;
	if (cfsetispeed(&attr, speed))
    {
        LOGE("cfsetispeed() error");
		close(fd); 
        return -1;
    }
    if (cfsetospeed(&attr, speed))
    {
        LOGE("cfsetospeed() error");
		close(fd); 
        return -1;  
    }
    
    attr.c_cc[VTIME]    = 100; // (100 * 0.1 sec) 10 second timeout
    attr.c_cc[VMIN]     = 0;   // 1 char satisfies read
    
    if (tcsetattr(fd, TCSANOW, &attr) < 0)
    {
        LOGE("error setting serial port settings");
        close(fd); 
        return -1;
    }
	return fd;
}
/*****************************************************************/
/*****************************************************************/
/*****                                                       *****/
/*****       I N T E R F A C E                               *****/
/*****                                                       *****/
/*****************************************************************/
/*****************************************************************/
GpsInterface *secInterface;
typedef void *(* getGpsInterface)(void);
int initialized=0;
static int
nowplus_gps_init(GpsCallbacks* callbacks)
{
    GpsState*  s = _gps_state;
    GpsCallbacks secCallbacks;
         const char *error;
	 void *module;
	 getGpsInterface sec_get_hardware_interface;
if(!initialized)
{
	 /* Load dynamically loaded library */
	 module = dlopen("libsecgps_orig.so", RTLD_LAZY);
	 if (!module) {
	   LOGE("Couldn't open libsecgps_orig.so: %s\n",
	   dlerror());
	   exit(1);
	 }
	 dlerror();
	 /* Get symbol */
	 sec_get_hardware_interface = dlsym(module, "gps_get_hardware_interface");
	 if ((error = dlerror())) {
	   LOGE( "Couldn't find hello: %s\n", error);
	   exit(1);
	 }
	 /* Now call the function in the DL library */
	 secInterface=(*sec_get_hardware_interface)();
	initialized=1;
}
    //Init SEC
    secInterface->init(&secCallbacks);
    if (!s->init)
        gps_state_init(s);
    if (s->fd < 0)
        return -1;
    s->callbacks = *callbacks;
    return 0;
}
static void
nowplus_gps_cleanup(void)
{
    GpsState*  s = _gps_state;
    if (s->init)
        gps_state_done(s);
    secInterface->cleanup();
}
static int
nowplus_gps_start()
{
    GpsState*  s = _gps_state;
    if (!s->init) {
        D("%s: called with uninitialized state !!", __FUNCTION__);
        return -1;
    }
    secInterface->start();
	
    D("%s: called", __FUNCTION__);
    gps_state_start(s);
    return 0;
}
static int
nowplus_gps_stop()
{
    GpsState*  s = _gps_state;
    if (!s->init) {
        D("%s: called with uninitialized state !!", __FUNCTION__);
        return -1;
    }
    D("%s: called", __FUNCTION__);
    gps_state_stop(s);
    secInterface->stop();
    return 0;
}
static int
nowplus_gps_inject_time(GpsUtcTime time, int64_t timeReference, int uncertainty)
{
    secInterface->inject_time(time,timeReference,uncertainty);	
    return 0;
}
static int
nowplus_gps_inject_location(double latitude, double longitude, float accuracy)
{
    secInterface->inject_location(latitude,longitude,accuracy);
    return 0;
}
static void
nowplus_gps_delete_aiding_data(GpsAidingData flags)
{
}
static int nowplus_gps_set_position_mode(GpsPositionMode mode, int fix_frequency)
{
   secInterface->set_position_mode(mode,fix_frequency);
    // FIXME - support fix_frequency
    return 0;
}
static const void*
nowplus_gps_get_extension(const char* name)
{
    return NULL;
}
static const GpsInterface  nowplusGpsInterface = {
    nowplus_gps_init,
    nowplus_gps_start,
    nowplus_gps_stop,
    nowplus_gps_cleanup,
    nowplus_gps_inject_time,
    nowplus_gps_inject_location,
    nowplus_gps_delete_aiding_data,
    nowplus_gps_set_position_mode,
    nowplus_gps_get_extension,
};
const GpsInterface* gps_get_hardware_interface()
{
         initialized=0;
	 return &nowplusGpsInterface;
}