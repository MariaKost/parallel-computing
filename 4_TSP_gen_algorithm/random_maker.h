#include <stddef.h>

struct RandomMaker;

struct RandomBlock;

extern const size_t kRandomQueueBlockSize;

typedef struct RandomMaker RandomMaker;

RandomMaker* RandomMakerCreate();
void RandomMakerDelete(RandomMaker* self);

void RandomMakerShutdown(RandomMaker* self);

unsigned* RandomMakerPopRandom(RandomMaker* self);

typedef struct RandomBlock RandomBlock;

RandomBlock* RandomBlockCreate(RandomMaker* maker);
void RandomBlockDelete(RandomBlock* self);

unsigned RandomBlockPopRandom(RandomBlock* self);

size_t RandomBlockPopRandomLong(RandomBlock* self);
