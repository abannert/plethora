enum state {
    ST_IDLE = 0,
    ST_CONNECTING,
    ST_CONNECTED,
    ST_WRITING,
    ST_WRITTEN,
    ST_READING_HEADER,
    ST_READING_BODY,
    ST_READ,
    ST_CLOSING,
    ST_CLOSED,
    ST_CALCULATING,
    ST_CLEANUP,
    ST_TIMEOUT,
    ST_ERROR,
};

struct connection;

void initialize_dispatcher();
