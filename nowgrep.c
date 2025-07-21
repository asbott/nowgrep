
#define OSTD_IMPL
#include "One-Std/one-headers/one_system.h"
#include "One-Std/one-headers/one_print.h"
#include "One-Std/one-headers/one_math.h"
#include "One-Std/one-headers/one_unicode.h"

// @todo move to ostd
u64 sys_get_current_thread_index(void) {
	if (!_ostd_thread_storage_register_mutex_initted) {
		// No other threads are initted, this has to be main thread.
		return 0;
	}
	
	if (sys_get_current_thread_id() == _ostd_main_thread_id) {
		return 0;
	}
	
	sys_mutex_acquire(_ostd_thread_storage_register_mutex);
	
	for (u64 i = 0; i < _ostd_thread_storage_allocated_count; i += 1) {
		_Ostd_Thread_Storage *s = &_ostd_thread_storage[i];
		if (s->thread_id == sys_get_current_thread_id()) {
			sys_mutex_release(_ostd_thread_storage_register_mutex);
			return i+1;
		}
	}
	
	sys_mutex_release(_ostd_thread_storage_register_mutex);
	assert(false);
	return U64_MAX;
}

typedef struct Thread_Profiler {
	Arena arena;
	bool initted;
} Thread_Profiler;

Thread_Profiler profilers[1024] = {0};

void dump_profiler_result(void) {
	
	File_Handle file = sys_open_file(STR("google_trace.json"), FILE_OPEN_CREATE | FILE_OPEN_RESET | FILE_OPEN_WRITE);
	sys_write_string(file, STR("["));
	
	for (u64 i = 0; i < 1024; i += 1) {
		Thread_Profiler *profiler = &profilers[i];
		if (profiler->initted && profiler->arena.position > profiler->arena.start) {
			sys_write_string(
				file, 
				(string){ (u64)profiler->arena.position - (u64)profiler->arena.start, (u8*)profiler->arena.start }
			);
		}
	}
	
	
	sys_write_string(file, STR("{}]"));
	
	sys_close(file);
}
void profiler_report_time(string name, f64 count, f64 start) {
	Thread_Profiler *profiler = &profilers[sys_get_current_thread_index()];
	
	if (!profiler->initted) {
		profiler->initted = true;
		profiler->arena = make_arena(GiB(8), MiB(16));
	}
	
	f64 dur = (float64)(count * 1000000);
	f64 ts = start * 1000000;
	u64 tid = sys_get_current_thread_id();
	
	//string fmt = STR("{\"cat\":\"function\",\"dur\":%f,\"name\":\"%s\",\"ph\":\"X\",\"pid\":0,\"tid\":%u,\"ts\":%f},");
	
	arena_push_string(&profiler->arena, STR("{\"cat\":\"function\",\"dur\":"));
	
	u8 buff[64] = {0};
	u64 n = format_float(dur, 3, buff, 64);
	arena_push_string(&profiler->arena, (string) { n, buff });
	
	arena_push_string(&profiler->arena, STR(",\"name\":\""));
	
	arena_push_string(&profiler->arena, name);
	
	arena_push_string(&profiler->arena, STR("\",\"ph\":\"X\",\"pid\":0,\"tid\":"));
	
	n = format_unsigned_int(tid, 10, buff, 64);
	arena_push_string(&profiler->arena, (string) { n, buff });
	
	arena_push_string(&profiler->arena, STR(",\"ts\":"));
	
	n = format_float(ts, 3, buff, 64);
	arena_push_string(&profiler->arena, (string) { n, buff });
	
	arena_push_string(&profiler->arena, STR("},"));
}

#define ENABLE_PROFILING

#ifdef ENABLE_PROFILING
#define tm_scope(name) \
  _Pragma("clang diagnostic push") \
  _Pragma("clang diagnostic ignored \"-Wshadow\"") \
  for (f64 start_time = sys_get_seconds_monotonic(), end_time = start_time, elapsed_time = 0; \
       elapsed_time == 0; \
       elapsed_time = (end_time = sys_get_seconds_monotonic()) - start_time, profiler_report_time(STR(name), elapsed_time, start_time)) \
  _Pragma("clang diagnostic pop")
#else
	#define tm_scope(...)
#endif



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
	ATTR_INDEX_ROOT = 0x90,
	
	ATTR_LOGGED_UTILITY_STREAM = 0x100,
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

typedef enum File_Record_Flags {
	FILE_RECORD_FLAGS_IN_USE = 0x0001,
	FILE_RECORD_FLAGS_DIRECTORY = 0x0002,
	FILE_RECORD_FLAGS_EXTENSION = 0x0004,
	FILE_RECORD_FLAGS_SPECIAL_INDEX = 0x0020,
	
} File_Record_Flags;

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

#pragma pack(pop)

// @todo compress this as much as possible
typedef struct Disk_Entry {
	struct Disk_Entry **pointer_to_parent_entry;
	string name;
	u8 *data; // Valid when resident.
	u64 size; // Size of file data if resident, otherwise size of data run table.
	bool resident; // If resident, offset is to raw data, if non resident, offset is to data runs
	bool is_directory;
} Disk_Entry;

typedef struct Volume_State {
	string name; // C, D, E etc
	
	Disk_Entry *entries;
	u64 entry_count;
	
	Disk_Entry **entries_per_record;
	
	u64 file_record_hole_count;
	u64 corrupt_usn_count;
	u64 compressed_count;
	u64 attr_list_count;
	u64 unnamed_count;
	u64 dataless_count;
	
} Volume_State;

typedef struct Record_Range {
	u64 first_record_index;
	u64 record_count;
} Record_Range;

#define BYTES_PER_RECORD_READBACK (MiB(64))
unit_local u64 max_indexing_memory_usage = GiB(3);
unit_local string search_directory;
unit_local bool has_search_directory = false;
unit_local string search_term;
unit_local bool has_search_term = false;
unit_local string filters[2048];
unit_local u64 filter_count;
unit_local bool is_entire_volume_search = false;
unit_local bool is_command_line = false;

bool index_volume(string disk_id, Volume_State *vol) {
	
	*vol = (Volume_State){0};
	
	string disk_file_name = tprint("\\\\.\\%s:", disk_id);
	
	u16 _cpath[MAX_PATH_LENGTH*2];
	u16 *cpath = _cpath;
	_win_utf8_to_wide(disk_file_name, cpath, MAX_PATH_LENGTH*2);
	File_Handle f = CreateFileW(
		cpath, GENERIC_READ,
		FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
		0, OPEN_EXISTING, FILE_FLAG_RANDOM_ACCESS, 0
	);
	File_Handle fasync = CreateFileW(
		cpath, GENERIC_READ,
		FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
		0, OPEN_EXISTING, FILE_FLAG_RANDOM_ACCESS | FILE_FLAG_OVERLAPPED, 0
	);
	
	if (f == (File_Handle)0xffffffffffffffff) {
		print("Could not open volume for reading.\n");
		return false;
	}
	
	BYTE sector[512];
	DWORD read = 0;
	tm_scope("ReadFile boot sector")
		ReadFile(f, sector, 512, &read, 0);
	
	Ntfs_Boot_Sector *boot = (Ntfs_Boot_Sector*)sector;
	
	u64 mft_offset = boot->mft_cluster_number * boot->sectors_per_cluster * boot->bytes_per_sector;
	
	// I have never come across a record_size which is not 1024. But I guess might as well do this.
	u64 record_size;
	s8  rc = boot->bytes_or_clusters_per_file_record_segment;
	if (rc < 0) {
		record_size = 1u << (-rc);
	} else {
		record_size = (u64)rc
		* boot->bytes_per_sector
		* boot->sectors_per_cluster;
	}
	
	LARGE_INTEGER mv;
	mv.QuadPart = (LONGLONG)mft_offset;
	LARGE_INTEGER new_pointer;
	SetFilePointerEx(f, mv, &new_pointer, 0/*FILE_BEGIN*/);
	
	u8* file_record_buffer = PushTempBuffer(u8, record_size+boot->bytes_per_sector);
	file_record_buffer = (u8*)(uintptr)align_next((u64)file_record_buffer, boot->bytes_per_sector);
	
	// Read first file record
	tm_scope("ReadFile first record")
		ReadFile(f, file_record_buffer, (DWORD)record_size, &read, 0);
	File_Record *rec = (File_Record*)file_record_buffer;
	strings_match((string){4, rec->magic}, STR("FILE"));
	
	Record_Range record_ranges[1024] = {0};
	u64 record_range_count = 0;
	u64 total_record_count = 0;
	
	{
		///
		// Assume first file record is $MFT and find all file record ranges
		
		File_Name *file_name_attrib = 0;
		Attribute_Header_Non_Resident *data_attrib = 0;
		
		Attribute_Header_Base *attr_base = (Attribute_Header_Base *)((u8*)rec + rec->first_attribute_offset);
		while (*(u32*)attr_base != 0xFFFFFFFF) {
			
			if (attr_base->type == ATTR_FILE_NAME)
			file_name_attrib = (File_Name *)((Attribute_Header_Resident *)attr_base)->attr_and_maybe_name;
			
			if (attr_base->type == ATTR_DATA) data_attrib = (Attribute_Header_Non_Resident *)attr_base;
			
			attr_base = (Attribute_Header_Base*)((u8*)attr_base + attr_base->length_including_header);
		}
		
		string name = string_allocate(get_temp(), file_name_attrib->file_name_length*3+1);
		name.count = (u64)WideCharToMultiByte(CP_UTF8, 0, (LPCWCH)file_name_attrib->name, (int)file_name_attrib->file_name_length, (char*)name.data, (int)name.count, 0, 0);
		
		// Assume $MFT/
		assert(file_name_attrib);
		assert(data_attrib);
		assert(strings_match(name, STR("$MFT")));
		
		///
		// Find ranges
		
		u8 *data_attr_start  = (u8*)data_attrib + data_attrib->offset_to_data_runs;
		u64 data_attr_size   = ((u64)data_attrib->base.length_including_header - (u64)data_attrib->offset_to_data_runs);
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
			previous_cluster  = start_cluster;
			
			u64 offset_in_bytes = start_cluster  * boot->sectors_per_cluster * boot->bytes_per_sector;
			u64 length_in_bytes = cluster_length * boot->sectors_per_cluster * boot->bytes_per_sector;
			
			assert(offset_in_bytes % record_size == 0);
			assert(length_in_bytes % record_size == 0);
			
			Record_Range *range = &record_ranges[record_range_count++];
			
			range->first_record_index = offset_in_bytes/record_size;
			range->record_count       = length_in_bytes/record_size;
			
			total_record_count += range->record_count;
			
			p += cluster_offset_size;
		}
	}
	
	//////////////////////
	//// Allocate a) records readback buffer (transient), and b) the memory to hold our indexed entries etc.
	
	u64 max_readback_memory = max(max_indexing_memory_usage, BYTES_PER_RECORD_READBACK);
	u64 max_readback_count = max_readback_memory/BYTES_PER_RECORD_READBACK;
	
	u64 ps = sys_get_info().page_size;
	
	u8 *records_buffer = 0;
	{
		u64 page_count = (max_readback_memory + ps + 1) / ps;
		records_buffer = (u8*)sys_map_pages(SYS_MEMORY_RESERVE | SYS_MEMORY_ALLOCATE, 0, page_count, false);
	}
	
	vol->entry_count = total_record_count;
	u64 entries_size = vol->entry_count*sizeof(Disk_Entry);
	u64 entries_per_record_size = vol->entry_count*sizeof(Disk_Entry*);
	u64 entries_mem_size = align_next(entries_size, 8) + entries_per_record_size;
	{
		u64 page_count  = (entries_mem_size + ps + 1) / ps;
		u8 *entries_mem = (u8*)sys_map_pages(SYS_MEMORY_RESERVE | SYS_MEMORY_ALLOCATE, 0, page_count, false);
		
		vol->entries = (Disk_Entry*)entries_mem;
		vol->entries_per_record = (Disk_Entry**)(uintptr)align_next((u64)(entries_mem + sizeof(Disk_Entry)*vol->entry_count), 8);
	}
	
	
	OVERLAPPED *ovl_buffer = PushTempBuffer(OVERLAPPED, max_readback_count);
	HANDLE *events = PushTempBuffer(HANDLE, max_readback_count);
	u64 *read_sizes = PushTempBuffer(u64, max_readback_count);
	memset(ovl_buffer, 0, sizeof(OVERLAPPED)*max_readback_count);
	memset(events, 0, sizeof(HANDLE)*max_readback_count);
	memset(read_sizes, 0, sizeof(u64)*max_readback_count);
	u64 readback_count = 0;
	u64 readback_next_position = 0;
	u64 readback_place_position = 0;
	
	for (u64 i = 0; i < max_readback_count; i += 1) {
		events[i] = CreateEventA(0, 1, 0, 0);
	}
	
	u64 processed_record_count = 0;
	
	for (u64 i = 0; i < record_range_count; i += 1) tm_scope("Record Range") {
		Record_Range range = record_ranges[i];
		
		//assert((page_count*ps) % record_size == 0);
		
		mv.QuadPart = (LONGLONG)(range.first_record_index*record_size);
		SetFilePointerEx(f, mv, &new_pointer, 0/*FILE_BEGIN*/);
		
		u64 records_bytes_read = 0;
		u64 records_bytes_queued = 0;
		
		while (records_bytes_read < range.record_count*record_size) tm_scope("Record Range step") {
			
			u8 *readback_data = 0; (void)readback_data;
			
			u64 max_queues_per_iter = 3;
			u64 queued_this_iter = 0;
			
			while (records_bytes_queued < range.record_count*record_size && queued_this_iter < max_queues_per_iter) tm_scope("Queue read") {
		
				if (readback_count >= max_readback_count) {
					break;
				}
			
				u64 remainder = (range.record_count*record_size) - records_bytes_queued;
				u64 to_read_async = (DWORD)min(remainder, BYTES_PER_RECORD_READBACK);
				
				if (to_read_async) tm_scope("Queue next ReadFile") {
					u8 *target_data = records_buffer + (readback_place_position%max_readback_count)*BYTES_PER_RECORD_READBACK;
					
					OVERLAPPED *ovl = ovl_buffer + (readback_place_position % max_readback_count);
					*ovl = (OVERLAPPED){0};
					u64 byte_offset = range.first_record_index*record_size + records_bytes_queued;
					ovl->Offset = byte_offset & 0xFFFFFFFF;
					ovl->OffsetHigh = byte_offset >> 32;
					ovl->hEvent = events[(readback_place_position % max_readback_count)];
					ResetEvent(events[(readback_place_position % max_readback_count)]);
					tm_scope("Async ReadFile call")
					ReadFile(fasync, target_data, (DWORD)to_read_async, 0, ovl);
					
					read_sizes[(readback_place_position % max_readback_count)] = to_read_async;
					
					sys_atomic_add_64(&readback_count, 1);
					readback_place_position += 1;
					
					records_bytes_queued += to_read_async;
					
					queued_this_iter += 1;
					
				} else continue;
			}
			
			/////
			// Queue next reads
			
			if (records_bytes_read >= range.record_count*record_size) break;
			
			tm_scope("Wait Last ReadFile") {
				OVERLAPPED *ovl = ovl_buffer + (readback_next_position % max_readback_count);
					
				DWORD async_read = 0;
				BOOL ok = GetOverlappedResult(fasync, ovl, &async_read, true);
				assert(ok);
				assert(read_sizes[(readback_next_position % max_readback_count)] == async_read);
				
				readback_data = records_buffer + (readback_next_position%max_readback_count)*BYTES_PER_RECORD_READBACK;
				
				readback_next_position += 1;
				sys_atomic_add_64(&readback_count, (u64)-1);
				
				
				records_bytes_read += async_read;
				
				read = async_read;
			}
			
			u64 to_process = read/record_size;
			
			tm_scope("Process streamed records")
			for (u64 j = 0; j < to_process; j += 1) {
				
				Disk_Entry *next_entry = vol->entries + (processed_record_count + j);
				
				// @speed we could get rid of this?
				memset(next_entry, 0, sizeof(Disk_Entry));
				
				rec = (File_Record*)(readback_data + record_size*(j));
				{
					/////
					// Apply ntfs fixups
					
					u16  *usa = (u16 *)((u8 *)rec + rec->offset_to_update_sequence);
					u16   usn = usa[0];
					u16   usa_count = rec->size_in_words_of_update_sequence;
					
					if (usa_count != record_size / boot->bytes_per_sector + 1) {
						vol->corrupt_usn_count += 1;
						continue;
					}
					
					for (u16 k = 1; k < usa_count; ++k) {
						u8 *sector_end = (u8 *)rec + k * boot->bytes_per_sector - 2;
						
						if (*(u16 *)sector_end != usn) {
							vol->corrupt_usn_count += 1;
							continue;
						}
						
						*(u16 *)sector_end = usa[k];
					}
				}
				
				if (!strings_match((string){4, rec->magic}, STR("FILE"))) {
					vol->file_record_hole_count += 1;
					continue;
				}
				
				next_entry->is_directory = (rec->flags & FILE_RECORD_FLAGS_DIRECTORY) != 0;
				bool has_attr_list = false;
				bool has_name = false;
				Attribute_Header_Base *attr_header_base = (Attribute_Header_Base *)((u8*)rec + rec->first_attribute_offset);
				
				while ((u64)attr_header_base < (u64)rec + rec->file_record_real_size && *(u32*)attr_header_base != 0xFFFFFFFF) {
					
					bool attr_named = attr_header_base->name_length > 0;
					bool resident = attr_header_base->non_resident == 0;
					
					if (attr_header_base->type == ATTR_FILE_NAME) {
						assert(!attr_named && resident);
						
						has_name = true;
						
						Attribute_Header_Resident *attr_header = (Attribute_Header_Resident *)attr_header_base;
						
						File_Name *attr = (File_Name *)(attr_header->attr_and_maybe_name);
						
						string name = string_allocate(get_temp(), attr->file_name_length*3+1);
						
						
						name.count = (u64)WideCharToMultiByte(CP_UTF8, 0, (LPCWCH)attr->name, (int)attr->file_name_length, (char*)name.data, (int)name.count, 0, 0);
						
						//print("Name: %s\n", name);
						
						next_entry->name = name;
						
						if (attr->parent_file_reference != 0) {
							u64 parent_index = attr->parent_file_reference & 0x0000ffffffffffff;
							next_entry->pointer_to_parent_entry = vol->entries_per_record + parent_index;
						}
						
						
					} else if (attr_header_base->type == ATTR_DATA) {
						assert(!next_entry->is_directory);
						next_entry->resident = resident;
						
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
					
					if (attr_header_base->type == ATTR_LOGGED_UTILITY_STREAM) {
						break;
					}
					
					if (attr_header_base->type == ATTR_ATTRIBUTE_LIST) {
						has_attr_list = true;
					}
					
					attr_header_base = (Attribute_Header_Base*)((u8*)attr_header_base + attr_header_base->length_including_header);
				}
				
				
				if (!(rec->flags & FILE_FLAGS_Compressed)) {
					vol->entries_per_record[processed_record_count+j] = next_entry;
					
					if (!next_entry->is_directory && !string_starts_with(next_entry->name, STR("$")) && !has_attr_list && has_name) {
						//assertmsgs(next_entry->data != 0, tprint("File '%s' has no data attribute.", next_entry->name));
						if (next_entry->data == 0) {
							vol->dataless_count += 1;
							//print("%s\n", next_entry->name);
						}
					}
				}
				
				if (rec->flags & FILE_FLAGS_Compressed) {
					vol->compressed_count += 1;
				}
				if (has_attr_list) {
					vol->attr_list_count += 1;
				}
				if (!has_name) {
					vol->unnamed_count += 1;
				}
			}
			
			processed_record_count += to_process;
		}
		if (!is_command_line)
			for (u64 k = 0; k < max_readback_count; k += 1) CloseHandle(events[k]);
	}
	
	if (!is_command_line) {
		sys_unmap_pages(records_buffer);
		CloseHandle(f);
		CloseHandle(fasync);
	}
	
	return true;
}

/*void update_volume_blocking(Volume_State *state, bool *keep_going_flag) {
	while (*keep_going_flag) {
		
	}
}*/

int main(int argc, char **argv) {
	search_directory = STR("");
	search_term = STR("*");
	
	if (argc == 1) {
		print("Goodbye, Gui.\n");
	} else {
		
		is_command_line = true;
		
		/////////////////
		/// Parse command line
		
		{
			string full_filter = STR("");
			for (int i = 1; i < argc; i += 1) {
				reset_temporary_storage();
				string item = STR(argv[i]);
				
				if (strings_match(item, STR("-d")) || strings_match(item, STR("--directory"))) {
					if (i >= argc-1) {
						print("Missing argument for switch '%s'\n", item);
						sys_exit(1);
					}
					
					++i;
					string arg0 = STR(argv[i]);
					
					search_directory = arg0;
					has_search_directory = true;
				} else if (strings_match(item, STR("-t")) || strings_match(item, STR("--term"))) {
					if (i >= argc-1) {
						print("Missing argument for switch '%s'\n", item);
						sys_exit(1);
					}
					
					++i;
					string arg0 = STR(argv[i]);
					
					search_term = arg0;
					has_search_term = true;
				} else if (strings_match(item, STR("-f")) || strings_match(item, STR("--filter"))) {
					if (i >= argc-1) {
						print("Missing argument for switch '%s'\n", item);
						sys_exit(1);
					}
					
					++i;
					string arg0 = STR(argv[i]);
					
					full_filter = arg0;
				} else {
					print("Unknown switch '%s'\n", item);
					sys_exit(1);
				}
			}
			
			if (!has_search_directory) {
				print("Missing search directory (-d, --directory)\n");
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
		}
		
		s64 colon_index = string_find_index_from_left(search_directory, STR(":"));
		
		if (colon_index == -1) {
			print("Invalid path '%s'\n", search_directory);
			sys_exit(1);
		}
		
		
		while (string_ends_with(search_directory, STR("\\"))) {
			search_directory.count -= 1;
		}
		
		if (string_count_occurences(search_directory, STR("\\")) == 0) {
			is_entire_volume_search = true;
		}
		
		string disk_id = string_slice(search_directory, 0, (u64)colon_index);
		
		//disk_file_name = tprint("\\\\.\\%s:", disk_id);
		
		Volume_State vol;
		bool vol_ok = index_volume(disk_id, &vol);
		
		if (!vol_ok) {
			print("Indexing volume failed.\n");
			sys_exit(1);
		}
		
		
		//for (u64 k = 0; k < vol.entry_count; k += 1) {
		//	print("%s\n", vol.entries[k].name);
		//}
		
		print("file_record_hole_count: %u\n", vol.file_record_hole_count);
		print("corrupt_usn_count: %u\n", vol.corrupt_usn_count);
		print("compressed_count: %u\n", vol.compressed_count);
		print("attr_list_count: %u\n", vol.attr_list_count);
		print("unnamed_count: %u\n", vol.unnamed_count);
		print("dataless_count: %u\n", vol.dataless_count);
		print("Total file records processed: %u\n", vol.entry_count);
		
		
		print("Goodbye, Terminal.\n");
	}
	
	dump_profiler_result();
	
	return 0;
}
