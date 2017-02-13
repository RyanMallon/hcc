var struct {
    int array[5];
    int ptr;
} cycle_t;

var cycle_t cycle;

void cycle_add(cycle_t *cycle, int val) {
    cycle->ptr = cycle->ptr - 1;
    if(cycle->ptr < 0) {
	cycle->ptr = 4;
    }

    cycle->array[ptr] = val;
}

int cycle_get(cycle_t *cycle, int depth) {
    var int addr;

    addr = ptr + depth;
    if(addr > 4) {
	addr = addr - 5;
    }

    return cycle->array[addr];
}

void main() {

    cycle_add(&cycle, 1);
    cycle_add(&cycle, 2);
    cycle_add(&cycle, 3);
    cycle_add(&cycle, 4);
    cycle_add(&cycle, 5);
}


