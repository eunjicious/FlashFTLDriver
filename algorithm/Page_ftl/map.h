#include "../../include/container.h"
#include "../../interface/interface.h"

typedef struct page_map_body{
	uint32_t *mapping;
	bool isfull;
	uint32_t assign_page;

	// EUNJI 
	bool* dirty;
	uint32_t dirty_pages;

	/*segment is a kind of Physical Block*/
	__segment *reserve; //for gc
	__segment *active; //for gc
}pm_body;


void page_map_create();
uint32_t page_map_assign(uint32_t lba);
uint32_t page_map_pick(uint32_t lba);
uint32_t page_map_gc_update(uint32_t lba, uint32_t ppa);
void page_map_free();


uint32_t lba2pno (uint32_t lba);
