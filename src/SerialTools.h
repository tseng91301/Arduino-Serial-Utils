#include <Arduino.h>
#include <QueueArray.h> // 需要安裝 Arduino 的 QueueArray library

template <typename SerialType>
class SerialUtils {
    private:
        SerialType* _serial;
        int _baud;

        uint8_t *buffer = nullptr;
        unsigned long buffer_max = 10;
        unsigned long buffer_length = 0;

        bool have_interrupt_byte = false;
        uint8_t interrupt_byte = 0;

        QueueArray<const char*> queue; // 儲存每次收到完整訊息

        int check_buffer_size() {
            if (buffer_length >= buffer_max) {
                buffer_max += 10;
                uint8_t* new_buf = (uint8_t*)realloc(buffer, buffer_max);
                if (new_buf) {
                    buffer = new_buf;
                    return 0;
                } else {
                    return 1;
                }
            }
            return 0;
        }

        void init(SerialType* serial, int baud) {
            _serial = serial;
            _baud = baud;
            _serial->begin(_baud);
            buffer = (uint8_t*)malloc(buffer_max);
        }

        void push_query() {
            // 確保 null terminator
            if (check_buffer_size()) return;
            buffer[buffer_length] = '\0';
            // 複製一份資料放入 queue
            char* msg = strdup((const char*)buffer);
            if (msg) queue.push(msg);
            buffer_length = 0; // 重置 buffer
        }

    public:
        SerialUtils(SerialType* serial, int baud) {
            init(serial, baud);
        }

        SerialUtils(SerialType* serial, int baud, uint8_t interrupt_byte): queue(10) {
            init(serial, baud);
            set_interrupt_byte(interrupt_byte);
        }

        ~SerialUtils() {
            free(buffer);
            while (!queue.isEmpty()) {
                free(queue.pop());
            }
        }

        void set_interrupt_byte(uint8_t b) {
            interrupt_byte = b;
            have_interrupt_byte = true;
        }

        void delete_buffer() {
            buffer_length = 0;
        }

        uint16_t queue_size() {
            return queue.count();
        }

        const char* get_buffer() {
            if (queue.isEmpty()) return nullptr;
            char* msg = queue.pop();
            return msg; // 呼叫端需要 free(msg)
        }

        void send_bytes(uint8_t *bytes, int length) {
            for(int i = 0; i < length; i++) {
                _serial->write(bytes[i]);
            }
            return;
        }

        void service() {
            // 設定 serial read 延時
            static unsigned long last_read_time = millis();
            unsigned long now_time = millis();
            if (now_time - last_read_time < 1) return;
            last_read_time = now_time;

            while (_serial->available()) {
                uint8_t c = _serial->read();
                if (have_interrupt_byte && c == interrupt_byte) {
                    push_query();
                } else {
                    if (check_buffer_size()) return;
                    buffer[buffer_length++] = c;
                }
            }
        }
};
