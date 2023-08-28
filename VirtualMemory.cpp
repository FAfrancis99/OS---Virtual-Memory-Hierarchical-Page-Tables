#include "VirtualMemory.h"
#include "PhysicalMemory.h"


void VMinitialize ()
{
  for (uint64_t i = 0; i < PAGE_SIZE; i++)
  {
    PMwrite (i, 0);
  }
}

void
check_if_max_page (word_t curr, uint64_t page_swapped_in, uint16_t p, int i, word_t &max_frame, uint64_t &max_page)
{
  int choice1, choice2, choice;
  choice1 = (int)(page_swapped_in - p);
  if (choice1 < 0){
    choice1 *= (-1);
  }
  choice2 = NUM_PAGES - choice1;
  if (choice2 < 0){
    choice2 *= (-1);
  }
  if (choice1 < choice2){
    choice = choice1;
  }
  else{
    choice = choice2;
  }
  if (choice > (int)max_page)
  {
    max_page = p;
    max_frame = curr*(word_t)PAGE_SIZE + i;
  }
}


void
select_page (word_t curr, uint64_t p, uint64_t page_swapped_in, word_t &p_frame,uint64_t &max_, int d)
{
  uint64_t current_p;
  for (uint64_t i = 0; i < PAGE_SIZE; i++)
  {
    word_t address;
    PMread ((curr << OFFSET_WIDTH) + i, &address);
    if (address != 0)
    {
      current_p = (p << OFFSET_WIDTH) + i;
      if (d < TABLES_DEPTH - 1){
        select_page(address, current_p, page_swapped_in,
                    p_frame, max_, d+1);
      }
      else
      {
        check_if_max_page (curr, page_swapped_in, current_p, (int)i, p_frame,max_);
      }
    }
  }
}

void
after_finding_the_address (uint64_t virtualAddress, uint64_t split, word_t address, int i, word_t current_frame)
{
  if (i < TABLES_DEPTH-1){
    for (int j = 0; j < PAGE_SIZE; j++)
    {
      PMwrite ((address << OFFSET_WIDTH)+ j, 0);
    }
  }
  else
  {
    PMrestore (address, virtualAddress /PAGE_SIZE);
  }
  PMwrite ((current_frame << OFFSET_WIDTH) + split, address);
}


int
check_for_priority3 (uint64_t virtualAddress, int &frame)
{
  word_t max_frame;
  uint64_t max_ = 0;
  select_page(0, 0, virtualAddress / PAGE_SIZE, frame, max_, 0);
  PMread(frame, &max_frame);
  PMevict(max_frame, max_);
  return max_frame;
}

void check_for_free_frame(uint64_t d, word_t curr_frame,
                          word_t to_link_with, word_t& max_frame_index,
                          word_t& add, int& is_found, int &parent)
{
  if(d == TABLES_DEPTH)
  {
    add = 0;
    return;
  }
  int temp_add = add;
  bool is_empty = true;
  for (uint64_t i = 0; i < PAGE_SIZE; i++){
    word_t new_address;
    PMread((curr_frame<<OFFSET_WIDTH) + i,&new_address);
    if (new_address == 0) continue;
    is_empty = false;
    if (max_frame_index < new_address){
      max_frame_index = new_address;
    }
    check_for_free_frame(d+1, new_address, to_link_with,
                         max_frame_index, temp_add, is_found, parent);
    if(!is_found)
    {
      if (temp_add!=0)
      {
        is_found = 1;
        add = temp_add;
        if(parent == -1)
        {
          parent = (int)((curr_frame << OFFSET_WIDTH) + i);
        }
      }
    }
  }
  if (curr_frame == to_link_with) return;
  if (is_empty){
    add = curr_frame;
  }
}

int
check_for_priorities1_2 (word_t current_frame, word_t &max_frame_index, int &is_found)
{
  int add= 0;
  is_found= 0;
  int parent = -1 ;
  check_for_free_frame(0,0, current_frame, max_frame_index, add, is_found, parent);
  if (parent != -1){
    PMread (parent, &add);
    PMwrite(parent,0);
  }
  return add;
}

word_t
check_address_zero (uint64_t virtualAddress, uint64_t split, word_t address, int i, int current_frame)
{
  if (address == 0)
  {
    word_t max_frame_index = 0;
    int is_found;
    word_t add = check_for_priorities1_2 (current_frame, max_frame_index,
                                          is_found);
    if (is_found){
      address= add;
    }
    else{
      if (max_frame_index + 1 < NUM_FRAMES)
      {
        address = max_frame_index + 1;
      }
      else
      {
        int max_;
        int frame_ = 0;
        max_ = check_for_priority3 (virtualAddress, frame_);
        PMwrite(frame_, 0);
        address= max_;
      }
    }
    after_finding_the_address (virtualAddress, split, address, i, current_frame);
  }
  return address;
}


int addressing (uint64_t virtualAddress)
{
  int height = TABLES_DEPTH;
  auto temp_address = virtualAddress >> (height * OFFSET_WIDTH);
  uint64_t split = temp_address % PAGE_SIZE;
  word_t address = 0;

  for (uint64_t i = 0; i < TABLES_DEPTH; i++)
  {
    int current_frame = address;
    PMread ((address << OFFSET_WIDTH) + split, &address);
    address = check_address_zero (virtualAddress, split, address, (int)i, current_frame);
    height -= 1;
    temp_address = virtualAddress >> (height * OFFSET_WIDTH);
    split = temp_address % PAGE_SIZE;
  }
  return address;
}


int VMread (uint64_t virtualAddress, word_t *value)
{
  if (virtualAddress >= VIRTUAL_MEMORY_SIZE){
    return 0;
  }
  uint64_t offset = virtualAddress % PAGE_SIZE;
  int address = addressing (virtualAddress);
  PMread ((address << OFFSET_WIDTH) + offset, value);
  return 1;
}

int VMwrite (uint64_t virtualAddress, word_t value)
{
  if (virtualAddress >= VIRTUAL_MEMORY_SIZE){
    return 0;
  }
  uint64_t offset = virtualAddress % PAGE_SIZE;
  int address = addressing (virtualAddress);
  PMwrite ((address << OFFSET_WIDTH) + offset, value);
  return 1;
}