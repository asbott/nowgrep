
#define OSTD_IMPL
#include "One-Std/one-headers/one_system.h"
#include "One-Std/one-headers/one_print.h"

#pragma pack(push,1)
typedef struct Ntfs_Boot_Sector{
    u8   jump[3];
    u8   oem[8];
    u16   bytes_per_sector;
    u8   sectors_per_cluster;
    u16   reserved_sectors;
    u8   unused1[3];
    u8   unused2[2];
    u8   media_descriptor;
    u8   unused3[2];
    u16  sectors_per_track;
    u16  number_of_heads;
    u32  hidden_sectors;
    u32  unused5;
    u32  unused6;
    u64  totalSectors;
    u64  mft_cluster_number;
    u64  mft_mirror_cluster_number;
    s8 bytes_or_clusters_per_file_record_segment;
    u8 unused7[3];
    u8 bytes_or_clusters_per_file_index_buffer;
    u8 unused8[3];
    u64 volume_serial_number;
    u32 checksum;
} Ntfs_Boot_Sector;

typedef enum Attribute_Type {
	ATTR_STANDARD_INFORMATION = 0x10,
	ATTR_ATTRIBUTE_LIST = 0x20,
	ATTR_FILE_NAME = 0x30,
	ATTR_SECURITY_DESCRIPTOR = 0x50,
	ATTR_DATA = 0x80,
} Attribute_Type;

typedef struct Attribute_Header_Base {
	Attribute_Type type;
	u32 length_including_header;
	u8 non_resident;
	u8 name_length;
	u16 name_offset;
	u16 flags;
	u16 attribute_id;
} Attribute_Header_Base;

typedef struct Attribute_Header_Resident {
	Attribute_Header_Base base;
	u32 attribute_length;
	u16 attribute_offset;
	u8 indexed;
	u8 padding;
	u8 attr_and_maybe_name[];
} Attribute_Header_Resident;

typedef struct Attribute_Header_Non_Resident {
	Attribute_Header_Base base;
	u64 starting_vcn;
	u64 last_vcn;
	u16 offset_to_data_runs;
	u16 compression_unit_size;
	u32 padding;
	u64 attribute_allocated_size;
	u64 attribute_real_size;
	u64 stream_initialized_data_size;
	u8 runs_and_maybe_name[];
} Attribute_Header_Non_Resident;

typedef struct Attribute_List {
	Attribute_Type type;
	u16 record_length;
	u8 name_length;
	u8 offset_to_name;
	u64 starting_vcn;
	u64 base_file_reference;
	u16 attribute_id;
	u16 name[];
} Attribute_List;

typedef enum File_Permission {
	FILE_PERMISSION_Read_Only = 0x0001,
	FILE_PERMISSION_Hidden = 0x0002,
	FILE_PERMISSION_System = 0x0004,
	FILE_PERMISSION_Archive = 0x0020,
	FILE_PERMISSION_Device = 0x0040,
	FILE_PERMISSION_Normal = 0x0080,
	FILE_PERMISSION_Temporary = 0x0100,
	FILE_PERMISSION_Sparse_File = 0x0200,
	FILE_PERMISSION_Reparse_Point = 0x0400,
	FILE_PERMISSION_Compressed = 0x0800,
	FILE_PERMISSION_Offline = 0x1000,
	FILE_PERMISSION_Not_Content_Indexed = 0x2000,
	FILE_PERMISSION_Encrypted = 0x4000,
} File_Permission;

typedef enum File_Flags {
FILE_FLAGS_Read_Only = 0x0001,
FILE_FLAGS_Hidden = 0x0002,
FILE_FLAGS_System = 0x0004,
FILE_FLAGS_Archive = 0x0020,
FILE_FLAGS_Device = 0x0040,
FILE_FLAGS_Normal = 0x0080,
FILE_FLAGS_Temporary = 0x0100,
FILE_FLAGS_Sparse_File = 0x0200,
FILE_FLAGS_Reparse_Point = 0x0400,
FILE_FLAGS_Compressed = 0x0800,
FILE_FLAGS_Offline = 0x1000,
FILE_FLAGS_Not_Content_Indexed = 0x2000,
FILE_FLAGS_Encrypted = 0x4000,
FILE_FLAGS_Directory = 0x10000000,
FILE_FLAGS_Index_View = 0x20000000,
} File_Flags;

typedef struct Standard_Information {
	u64 ctime;
	u64 atime;
	u64 mtime;
	u64 rtime;
	u32 dos_file_permission;
	u32 maximum_number_of_versions;
	u32 version_number;
	u32 class_id;
	u32 owner_id;
	u32 security_id;
	u64 quota_changed;
	u64 usn;
} Standard_Information;

typedef struct File_Name {
	u64 parent_file_reference;
	u64 ctime;
	u64 atime;
	u64 mtime;
	u64 rtime;
	u64 allocated_size;
	u64 real_size;
	u32 flags;
	u32 unused;
	u8 file_name_length; // in characters
	u8 file_name_namespace;
	u16 name[];
} File_Name;

typedef struct File_Record {
	u8 magic[4]; // F I L E
	u16 offset_to_update_sequence;
	u16 size_in_words_of_update_sequence;
	u64 log_file_sequence_number;
	u16 sequence_number;
	u16 hard_link_count;
	u16 first_attribute_offset;
	u16 flags;
	u32 file_record_real_size;
	u32 file_record_allocated_size;
	u64 base_file_record_reference;
	u16 next_attribute_id;
	u16 pad;
	u32 mft_record_number;
	u8 update_sequence[];
} File_Record;

typedef struct Disk_Entry {
	u64 id;
    u64 parent_id;
    string name;
    u8 *data; // Valid when resident.
    u64 size; // Size of file data if resident, otherwise size of data run table.
    bool resident; // If resident, offset is to raw data, if non resident, offset is to data runs
    bool is_directory;
} Disk_Entry;

typedef struct Frn_To_Index_Mapping {
	u64 frn;
	u64 index;
} Frn_To_Index_Mapping;

#pragma pack(pop)

unit_local Ntfs_Boot_Sector *boot;
unit_local string disk_name;

unit_local string search_directory;
unit_local bool has_search_directory = false;
unit_local string search_term;
unit_local bool has_search_term = false;

unit_local Semaphore work_semaphore;
unit_local Arena work_arena;
unit_local volatile Disk_Entry *work;
unit_local volatile u64 work_count = 0;
unit_local volatile u64 worked_count = 0;
unit_local bool work_probe_done = false;

unit_local string full_filter;
unit_local string filters[2048];
unit_local u64 filter_count;

unit_local u64 bad_counter = 0;

unit_local u64 match_count = 0;
unit_local u64 filename_match_count = 0;

typedef struct Record_Range {
	u64 first_record_index;
	u64 record_count;
} Record_Range;

static bool ntfs_apply_fixups(File_Record *rec, u64 record_size, u16 bytes_per_sector)
{
    u16  *usa = (u16 *)((u8 *)rec + rec->offset_to_update_sequence);
    u16   usn = usa[0];
    u16   usa_count = rec->size_in_words_of_update_sequence;

    if (usa_count != record_size / bytes_per_sector + 1)
        return false;

    for (u16 i = 1; i < usa_count; ++i) {
        u8 *sector_end = (u8 *)rec + i * bytes_per_sector - 2;
        if (*(u16 *)sector_end != usn)
            return false;
        *(u16 *)sector_end = usa[i];
    }
    return true;
}

s64 worker_proc(Thread *t) {

	u16 _cpath[MAX_PATH_LENGTH*2];
    u16 *cpath = _cpath;
    _win_utf8_to_wide(disk_name, cpath, MAX_PATH_LENGTH*2);
    File_Handle f = CreateFileW(
        cpath, GENERIC_READ,
        FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
        0, OPEN_EXISTING, FILE_FLAG_RANDOM_ACCESS | FILE_FLAG_NO_BUFFERING, 0
    );
    
    assert(f != (void*)0xffffffffffffffff);
    
	u64 thread_index = *(u64*)t->userdata; (void)thread_index;
	
	u64 ps = sys_get_info().page_size;
	u64 read_size = MiB(100);
	u64 page_count = (read_size+ps+1)/ps;
	u8 *read_buffer = sys_map_pages(SYS_MEMORY_RESERVE | SYS_MEMORY_ALLOCATE, 0, page_count, false);
	(void)read_buffer;
	
	while (1) {
		
		sys_semaphore_wait(work_semaphore);
		
		if (work_probe_done && worked_count >= work_count) {
			break;
		}
		
		volatile u64 work_index = sys_atomic_add_64(&worked_count, 1);
		
		volatile Disk_Entry *entry = work + work_index;
		
		bool name_printed = false;
		
		if (entry->resident) {
			string content = (string) { entry->size, entry->data };
			s64 index;
			while ((index = string_find_index_from_left(content, search_term)) != -1) {
				if (!name_printed) {
					//name_printed = true;
					u64 view_index = (u64)(max(0, index-12));
					u64 view_length = min(12+search_term.count+12, content.count-view_index-1);
					string view_string = string_slice(content, view_index, view_length);
					
					sys_atomic_add_64(&match_count, 1);
					
					print("%s:, Small:\n\"%s\"\n\n", entry->name, view_string);
					
					sys_atomic_add_64(&match_count, 1);
					
					content = string_slice(content, (u64)index+search_term.count, content.count-(u64)index-1);
				}
			}
		} else {

    		u64 previous_cluster = 0;
			u8 *p = (u8*)entry->data;
		    while (*p != 0) {
		    	bool baba = p == (u8*)entry->data;
		    	
		    	assert((u64)((u64)p - (u64)entry->data) < entry->size);
		    	
		    	u8 header_byte = *p;
		    	volatile u64 cluster_length_size = header_byte & 0x0F;
		    	volatile u64 cluster_offset_size = header_byte >> 4;
		    	p += 1;
		    	assert((u64)((u64)p - (u64)entry->data) < entry->size);
		    	
		    	assertmsgs(cluster_length_size != 0, tprint("%u, %s, %b", cluster_offset_size, entry->name, baba));
		    	
		    	volatile u64 cluster_length = 0;
		    	volatile s64 cluster_offset = 0;
				
		    	memcpy((void*)(volatile void*)&cluster_length, p, min(cluster_length_size, 8));
		    	p += cluster_length_size;
		    	assertmsgs((u64)((u64)p - (u64)entry->data) <= entry->size, tprint("%u, %s, %b", cluster_offset_size, entry->name, baba));
				if (cluster_offset_size == 0) {
					sys_atomic_add_64(&bad_counter, 1);
					continue;
		    	}
		    	
		    	memcpy((void*)(volatile void*)&cluster_offset, p, min(cluster_offset_size, 8));
		    	assertmsgs((u64)((u64)p - (u64)entry->data) <= entry->size, tprint("%u, %s, %b", cluster_offset_size, entry->name, baba));
				
				assert(cluster_length*boot->sectors_per_cluster*boot->bytes_per_sector < TiB(30));
				
				assertmsgs(cluster_length != 0, tprint("%u, %s, %b", cluster_offset_size, entry->name, baba));
		    	
		    	// Sign extend
				if (cluster_offset_size < 8) {
				    if (p[cluster_offset_size - 1] & 0x80) {
				        cluster_offset |= (~(s64)0) << (cluster_offset_size * 8);
				    }
				}
		    	p += cluster_offset_size; // Must be after sign extend
				
				u64 start_cluster = (u64)((s64)previous_cluster + cluster_offset);
				previous_cluster = start_cluster;
				
				u64 offset_in_bytes = start_cluster * boot->sectors_per_cluster * boot->bytes_per_sector;
				u64 length_in_bytes = cluster_length * boot->sectors_per_cluster * boot->bytes_per_sector;
				
				LARGE_INTEGER mv;
				LARGE_INTEGER new_pointer;
				mv.QuadPart = (LONGLONG)(offset_in_bytes);
				SetFilePointerEx(f, mv, &new_pointer, 0/*FILE_BEGIN*/);
				
				u64 read_so_far = 0;
				bool error = false;
				while (read_so_far < length_in_bytes) {
					u64 to_read = min(length_in_bytes-read_so_far, read_size);
					read_so_far += to_read;
					DWORD read;
					ReadFile(f, read_buffer, (DWORD)to_read, &read, 0);
					if (read != to_read) {
						error = true;
						break;
					}
					string content = (string) { to_read, read_buffer };
					s64 index;
					while ((index = string_find_index_from_left(content, search_term)) != -1) {
						if (!name_printed) {
							//name_printed = true;
							
							u64 view_index = (u64)(max(0, index-12));
							u64 view_length = min(12+search_term.count+12, content.count-view_index-1);
							string view_string = string_slice(content, view_index, view_length);
							
							sys_atomic_add_64(&match_count, 1);
							
							print("%s:, Large:\n\"%s\"\n\n", entry->name, view_string);
							
							(void)name_printed;
							
							content = string_slice(content, (u64)index+search_term.count, content.count-(u64)index-1);
						}
					}
				}
				
				if (error) {
					break;
				}
				
				if (p == (u8*)entry->data + entry->size && *p != 0) assert(false);
			}
		}
		
	}
	return 0;
}

int main(int argc, char **argv) {

	f64 t0, t1, time;

	sys_semaphore_init(&work_semaphore);
	work_arena = make_arena(GiB(16), MiB(30));
	work = (Disk_Entry*)work_arena.start;

	search_directory = STR("");
	search_term = STR("");
	full_filter = STR("");
	
	for (int i = 1; i < argc; i += 1) {
		string item = STR(argv[i]);
		
		if (strings_match(item, STR("-d")) || strings_match(item, STR("--directory"))) {
			if (i >= argc-1) {
				print("Missing argument for switch '%s'\n", item);
				return -1;
			}
			
			++i;
			string arg0 = STR(argv[i]);
			
			search_directory = arg0;
			has_search_directory = true;
		} else if (strings_match(item, STR("-t")) || strings_match(item, STR("--term"))) {
			if (i >= argc-1) {
				print("Missing argument for switch '%s'\n", item);
				return -1;
			}
			
			++i;
			string arg0 = STR(argv[i]);
			
			search_term = arg0;
			has_search_term = true;
		} else if (strings_match(item, STR("-f")) || strings_match(item, STR("--filter"))) {
			if (i >= argc-1) {
				print("Missing argument for switch '%s'\n", item);
				return -1;
			}
			
			++i;
			string arg0 = STR(argv[i]);
			
			full_filter = arg0;
		} else {
			print("Unknown switch '%s'\n", item);
			return -1;
		}
	}
	
	if (!has_search_directory) {
		print("Missing search directory (-d, --directory)\n");
		sys_exit(1);
	}
	if (!has_search_term) {
		print("Missing search term (-t, --term)\n");
		sys_exit(1);
	}
	
	s64 index;
	while ((index = string_find_index_from_left(full_filter, STR(","))) != -1) {
		string filter = string_trim(string_slice(full_filter, 0, (u64)index));
		if (filter.count > 0) filters[filter_count++] = filter;
		full_filter = string_slice(full_filter, (u64)index+1, full_filter.count-(u64)index-1);
	}
	full_filter = string_trim(full_filter);
	if (full_filter.count > 0) filters[filter_count++] = full_filter;
	
	if (!sys_get_absolute_path(get_temp(), search_directory, &search_directory)) {
		print("Invalid path '%s'\n", search_directory);
		sys_exit(1);
	}
	
	s64 colon_index = string_find_index_from_left(search_directory, STR(":"));
	
	if (colon_index == -1) {
		print("Invalid path '%s'\n", search_directory);
		sys_exit(1);
	}
	
	string disk_id = string_slice(search_directory, 0, (u64)colon_index);
	
	disk_name = tprint("\\\\.\\%s:", disk_id);
	
    u16 _cpath[MAX_PATH_LENGTH*2];
    u16 *cpath = _cpath;
    _win_utf8_to_wide(disk_name, cpath, MAX_PATH_LENGTH*2);
    File_Handle f = CreateFileW(
        cpath, GENERIC_READ,
        FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
        0, OPEN_EXISTING, FILE_FLAG_RANDOM_ACCESS | FILE_FLAG_NO_BUFFERING, 0
    );
    
    if (f == (File_Handle)0xffffffffffffffff) return false;
    
    BYTE sector[512];
    DWORD read;
    ReadFile(f, sector, 512, &read, 0);
    
    boot = (Ntfs_Boot_Sector*)sector;
    
    u64 mft_offset = boot->mft_cluster_number * boot->sectors_per_cluster * boot->bytes_per_sector;
    
    
    u64 record_size;
    s8  rc = boot->bytes_or_clusters_per_file_record_segment;
	if (rc < 0) {
		// Gippity told me to do this
	    record_size = 1u << (-rc);
	} else {
	    record_size = (u64)rc
	               * boot->bytes_per_sector
	               * boot->sectors_per_cluster;
	}
    
    Arena record_range_arena = make_arena(GiB(10), KiB(32));
    Record_Range *record_ranges = (Record_Range *)record_range_arena.start;
    u64 record_range_count = 0;
    (void)record_ranges;
    
    LARGE_INTEGER mv;
    mv.QuadPart = (LONGLONG)mft_offset;
    LARGE_INTEGER new_pointer;
    
    SetFilePointerEx(f, mv, &new_pointer, 0/*FILE_BEGIN*/);
    
    t0 = sys_get_seconds_monotonic();
    
    u8* file_record_buffer = PushTempBuffer(u8, record_size+boot->bytes_per_sector);
    
    // Must be aligned to sector
    file_record_buffer = (u8*)(uintptr)align_next((u64)file_record_buffer, boot->bytes_per_sector);
    
    // Read first file record
    ReadFile(f, file_record_buffer, (DWORD)record_size, &read, 0);
    
    File_Record *rec = (File_Record*)file_record_buffer;
    
    bool found_mft = false;
    
    ///////////
	/// Run through metafiles and find MFT data run descriptors
    	
    while (strings_match((string){4, rec->magic}, STR("FILE"))) {
    	Attribute_Header_Base *attr_header_base = (Attribute_Header_Base *)((u8*)rec + rec->first_attribute_offset);
    	
    	bool is_mft = false;
    	
    	while (*(u32*)attr_header_base != 0xFFFFFFFF) {
    	
	    	bool attr_named = attr_header_base->name_length > 0;
	    	bool resident = attr_header_base->non_resident == 0;
	    	
	    	if (attr_header_base->type == ATTR_FILE_NAME) {
	    		assert(!attr_named && resident);
	    		
	    		Attribute_Header_Resident *attr_header = (Attribute_Header_Resident *)attr_header_base;
	    		
	    		File_Name *attr = (File_Name *)(attr_header->attr_and_maybe_name);
	    		
	    		string name = string_allocate(get_temp(), attr->file_name_length*3+1);
	    		
	    		name.count = (u64)WideCharToMultiByte(CP_UTF8, 0, (LPCWCH)attr->name, (int)attr->file_name_length, (char*)name.data, (int)name.count, 0, 0);
	    		if (strings_match(name, STR("$MFT"))) {
	    			found_mft = true;
	    			is_mft = true;
	    		}
	    	} else if (attr_header_base->type == ATTR_DATA) {

	    		if (is_mft) {
		    		Attribute_Header_Non_Resident *attr_header = (Attribute_Header_Non_Resident *)attr_header_base;
		    		
		    		u8 *data_attr_start = (u8*)attr_header + attr_header->offset_to_data_runs;
		    		u64 data_attr_size = ((u64)attr_header->base.length_including_header - (u64)attr_header->offset_to_data_runs);
		    		u64 previous_cluster = 0;
					u8 *p = (u8*)data_attr_start;
				    while (*p != 0) {
				    
				    	assert((u64)p < (u64)data_attr_start + data_attr_size);
				    	
				    	u8 header_byte = *p;
				    	u64 cluster_length_size = header_byte & 0x0F;
				    	u64 cluster_offset_size = header_byte >> 4;
				    	
				    	p += 1;
				    	
				    	u64 cluster_length = 0;
				    	s64 cluster_offset = 0;
				    	
				    	assert(cluster_length_size <= 8);
				    	assert(cluster_offset_size <= 8);
				    	
				    	memcpy(&cluster_length, p, min(cluster_length_size, 8));
				    	p += cluster_length_size;
				    	memcpy(&cluster_offset, p, min(cluster_offset_size, 8));
				    	
						if (cluster_offset_size < 8) {
						    if (p[cluster_offset_size - 1] & 0x80) {
						        cluster_offset |= (~(s64)0) << (cluster_offset_size * 8);
						    }
						}
						
						u64 start_cluster = (u64)((s64)previous_cluster + cluster_offset);
						previous_cluster = start_cluster;
						
						u64 offset_in_bytes = start_cluster * boot->sectors_per_cluster * boot->bytes_per_sector;
						u64 length_in_bytes = cluster_length * boot->sectors_per_cluster * boot->bytes_per_sector;
						
						assert(offset_in_bytes % record_size == 0);
						assert(length_in_bytes % record_size == 0);
						
						Record_Range *range = (Record_Range *)arena_push(&record_range_arena, sizeof(Record_Range));
						record_range_count += 1;
						range->first_record_index = offset_in_bytes/record_size;
						range->record_count = length_in_bytes/record_size;
						
				    	p += cluster_offset_size;
				    }
	    		}
	    	}
	    	
	    	attr_header_base = (Attribute_Header_Base*)((u8*)attr_header_base + attr_header_base->length_including_header);
    	}
    	
    	ReadFile(f, file_record_buffer, (DWORD)record_size, &read, 0);
    	
    }
    
    
    if (!found_mft) {
    	return false;
    }
    
    t1 = sys_get_seconds_monotonic();
    time = t1 - t0;
    print("Finding $MFT data runs: %f seconds\n", time);
    
    t0 = sys_get_seconds_monotonic();
    
    u64 hole_count = 0;
    
    u64 total_file_count = 0;
    u64 total_bytes_read = 0;
    
    void *mapped_pointers[4096];
    u64 mapped_pointer_count = 0;
    
    
    u64 total_record_count = 0;
    for (u64 i = 0; i < record_range_count; i += 1) {
    	total_record_count += record_ranges[i].record_count;
    }
    
	u64 ps = sys_get_info().page_size;
	u64 page_count = (total_record_count*record_size + ps + 1) / ps;
	
    Frn_To_Index_Mapping *frn_to_index_mapping = (Frn_To_Index_Mapping *)sys_map_pages(SYS_MEMORY_RESERVE | SYS_MEMORY_ALLOCATE, 0, page_count, false);
    (void)frn_to_index_mapping;
    
    Disk_Entry *next_entry = (Disk_Entry*)arena_push(&work_arena, sizeof(Disk_Entry));
    File_Handle test = sys_open_file(STR("files.txt"), FILE_OPEN_WRITE | FILE_OPEN_CREATE | FILE_OPEN_RESET);
	for (u64 i = 0; i < record_range_count; i += 1) {
		Record_Range range = record_ranges[i];
		
		u64 records_byte_size = range.record_count * record_size;
		page_count = (records_byte_size + ps + 1) / ps;
		
		u8 *record_data = sys_map_pages(SYS_MEMORY_RESERVE | SYS_MEMORY_ALLOCATE, 0, page_count, false);
		if (!record_data) return false;
		
		mapped_pointers[mapped_pointer_count++] = record_data;
		
		assert((page_count*ps) % record_size == 0);
		
		mv.QuadPart = (LONGLONG)(range.first_record_index*record_size);
		SetFilePointerEx(f, mv, &new_pointer, 0/*FILE_BEGIN*/);
		ReadFile(f, record_data, (DWORD)records_byte_size, &read, 0);
		if (read != (DWORD)records_byte_size) return false;
		
		
		for (u64 j = 0; j < range.record_count; j += 1) {
			rec = (File_Record*)(record_data + record_size*j);
			ntfs_apply_fixups(rec, record_size, boot->bytes_per_sector);
			
			if (!strings_match((string){4, rec->magic}, STR("FILE"))) {
				hole_count += 1;
				continue;
			}
			
			bool any_match = false;
			next_entry->is_directory = (rec->flags & FILE_FLAGS_Directory) != 0;
			next_entry->id = rec->base_file_record_reference;
			
			
			total_file_count += 1;
			
			Attribute_Header_Base *attr_header_base = (Attribute_Header_Base *)((u8*)rec + rec->first_attribute_offset);
			
			while ((u64)attr_header_base < (u64)rec + rec->file_record_real_size && *(u32*)attr_header_base != 0xFFFFFFFF) {
				
				bool attr_named = attr_header_base->name_length > 0;
				bool resident = attr_header_base->non_resident == 0;
				
				if (attr_header_base->type == ATTR_FILE_NAME) {
					assert(!attr_named && resident);
					
					Attribute_Header_Resident *attr_header = (Attribute_Header_Resident *)attr_header_base;
					
					File_Name *attr = (File_Name *)(attr_header->attr_and_maybe_name);
					
					string name = string_allocate(get_temp(), attr->file_name_length*3+1);
					
					
					name.count = (u64)WideCharToMultiByte(CP_UTF8, 0, (LPCWCH)attr->name, (int)attr->file_name_length, (char*)name.data, (int)name.count, 0, 0);
					
					for (u64 k = 0; k < filter_count; k += 1) {
						string filter = filters[k];
						bool prefix_wildcard = filter.data[0] == '*';
						bool suffix_wildcard = filter.data[filter.count-1] == '*';
						
						if (!prefix_wildcard && !suffix_wildcard) {
							if (strings_match(filter, name)) {
								any_match = true;
								break;
							}
						}
						
						if (prefix_wildcard && !suffix_wildcard) {
							string end = filter;
							end.data += 1;
							end.count -= 1;
							
							if (string_ends_with(name, end)) {
								any_match = true;
								break;
							}
						}
						
						if (!prefix_wildcard && suffix_wildcard) {
							string start = filter;
							start.count -= 1;
							
							if (string_starts_with(name, start)) {
								any_match = true;
								break;
							}
						}
						
						if (prefix_wildcard && suffix_wildcard) {
							string middle = filter;
							middle.count -= 2;
							middle.data += 1;
							
							if (string_find_index_from_left(name, middle) != -1) {
								any_match = true;
								break;
							}
						}
						
					}
					
					next_entry->name = name;
					next_entry->parent_id = attr->parent_file_reference;
					
					
				} else if (attr_header_base->type == ATTR_DATA) {
					assert(!next_entry->is_directory);
					next_entry->resident = resident;
					
					//assert(!attr_named);
					
					if (resident) {
						Attribute_Header_Resident *attr_header = (Attribute_Header_Resident *)attr_header_base;
						next_entry->size = attr_header->attribute_length;
						next_entry->data = (u8*)attr_header + attr_header->attribute_offset;
					} else {
						Attribute_Header_Non_Resident *attr_header = (Attribute_Header_Non_Resident *)attr_header_base;
						next_entry->size = ((u64)attr_header->base.length_including_header - (u64)attr_header->offset_to_data_runs);
						next_entry->data = (u8*)attr_header + attr_header->offset_to_data_runs;
					}
					
				}
				
				attr_header_base = (Attribute_Header_Base*)((u8*)attr_header_base + attr_header_base->length_including_header);
			}
			
			if (any_match && next_entry->data && !(rec->flags & FILE_FLAGS_Compressed)) {
			
				filename_match_count += 1;
			
				sys_atomic_add_64(&work_count, 1);
				
				sys_semaphore_signal(&work_semaphore);
				assert(next_entry->data != 0);
				next_entry = (Disk_Entry*)arena_push(&work_arena, sizeof(Disk_Entry));
			}
		}
		
	}
	
	t1 = sys_get_seconds_monotonic();
    time = t1 - t0;
    print("Filtering & indexing NTFS records: %f seconds\n", time);
	
	//sys_exit(0);
	
	work_probe_done = true;
	
	u64 worker_count = max((sys_get_info().logical_cpu_count/4)*3, 1);
    //u64 worker_count = sys_get_info().logical_cpu_count*3;
    
    Thread *threads = PushTempBuffer(Thread, worker_count);
	for (u64 i = 0; i < worker_count; i += 1) {
    	Thread *t = threads + i;
    	u64 *pi = PushTemp(u64);
    	*pi = i;
    	sys_thread_init(t, worker_proc, pi);
    	sys_thread_start(t);
    }
	
	for (u64 i = 0; i < worker_count; i += 1) {
    	sys_semaphore_signal(&work_semaphore);
    }
	for (u64 i = 0; i < worker_count; i += 1) {
    	Thread *t = threads + i;
    	sys_thread_join(t);
    }
    
    for (u64 i = 0; i < mapped_pointer_count; i += 1) {
		sys_unmap_pages(mapped_pointers[i]);
    }
	
	(void)hole_count;(void)total_file_count;(void)total_bytes_read;
	print("Holes: %u\n", hole_count);
	print("File Count: %u\n", total_file_count);
	print("Matched file count: %u\n", filename_match_count);
	print("Matches found: %u\n", match_count);
	print("Bad: %u\n", bad_counter);
	sys_close(test);
	
	
	print("Goodbye\n");
	return 0;
}

