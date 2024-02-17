#define MAX_CLIENTS 20
#define FROM_LEN 5
#define CONTENT_LEN 20
#define RECORD_NUM 10
#define RECORD_PATH "./BulletinBoard"

typedef struct {
 char From[FROM_LEN];
 char Content[CONTENT_LEN];
} record;
