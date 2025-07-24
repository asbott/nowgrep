
#define OSTD_IMPL
#include "One-Std/one-headers/one_system.h"
#include "One-Std/one-headers/one_print.h"
#include "One-Std/one-headers/one_math.h"
#include "One-Std/one-headers/one_unicode.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreserved-identifier"
#pragma clang diagnostic ignored "-Wdocumentation"
#pragma clang diagnostic ignored "-Wdocumentation-unknown-command"
#pragma clang diagnostic ignored "-Wdouble-promotion"
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wswitch-default"
#pragma clang diagnostic ignored "-Wimplicit-int-float-conversion"
#pragma clang diagnostic ignored "-Wbad-function-cast"
#pragma clang diagnostic ignored "-Wimplicit-fallthrough"
#pragma clang diagnostic ignored "-Wextra-semi-stmt"
#pragma clang diagnostic ignored "-Wswitch-enum"
#pragma clang diagnostic ignored "-Wfloat-conversion"
#pragma clang diagnostic ignored "-Wimplicit-int-conversion"
#pragma clang diagnostic ignored "-Wint-conversion"
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wint-to-pointer-cast"
#pragma clang diagnostic ignored "-Wunused-function"
#define NK_INT8 s8
#define NK_UINT8 u8
#define NK_INT16 s16
#define NK_UINT16 u16
#define NK_INT32 s32
#define NK_UINT32 u32
#define NK_SIZE_TYPE u64
#define NK_POINTER_TYPE uintptr

#define _In_
#define _Check_Return

#define NK_IMPLEMENTATION
#define NK_GDI_IMPLEMENTATION
#define NKGDI_IMPLEMENT_WINDOW
#define CALLBACK

#define STBRP_ASSERT assert

static void swap_bytes(char *a, char *b, size_t size)
{
	while (size--)
	{
		char tmp = *a;
		*a++ = *b;
		*b++ = tmp;
	}
}

static void qsort_recursive(char *base,
	size_t n,
	size_t size,
	int (*compar)(const void *, const void *))
{
	if (n < 2)
	return;
	
	size_t pivot_index = n / 2;
	char *pivot = base + pivot_index * size;
	
	swap_bytes(pivot, base + (n - 1) * size, size);
	pivot = base + (n - 1) * size;
	
	size_t store = 0;
	for (size_t i = 0; i < n - 1; ++i)
	{
		char *elem = base + i * size;
		if ((*compar)(elem, pivot) < 0)
		{
			swap_bytes(elem, base + store * size, size);
			++store;
		}
	}
	swap_bytes(base + store * size, pivot, size);
	
	qsort_recursive(base,                store,              size, compar);
	qsort_recursive(base + (store + 1) * size,
		n - store - 1,       size, compar);
}

void qsort(void *base,
	size_t nitems,
	size_t size,
	int (*compar)(const void *, const void *))
{
	if (!base || !compar || size == 0 || nitems < 2)
	return;
	
	qsort_recursive((char *)base, nitems, size, compar);
}
#define STBRP_SORT qsort

#define STBTT_ifloor(x) ((int)floor64((float64)x))
#define STBTT_iceil(x)  ((int) ceil64(x))

#define STBTT_fmod(x, y) fmod_cycling(x, y)

#define STBTT_cos cos
#define STBTT_fabs abs

#define STBTT_malloc(x,u)  ((void)(u),HeapAlloc(GetProcessHeap(), 0, x))
#define STBTT_free(x,u)    ((void)(u),HeapFree(GetProcessHeap(), 0, x))

#define STBTT_assert assert

#define STBTT_strlen c_style_strlen

#define STBTT_memcpy       memcpy
#define STBTT_memset       memset

#define SIZE_T u64

#define NK_INCLUDE_DEFAULT_ALLOCATOR

#include "Nuklear/nuklear.h"
#include "Nuklear/nuklear_gdi.h"
#include "Nuklear/window.h"


#pragma clang diagnostic pop

#define MAX_PATH     512
#define MAX_FILTER   256
#define MAX_SEARCH   256
#define MAX_RESULTS 1024



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
	u64 file_position; // If resident, position for file data, otherwise position for data run entries
	u64 size; // Size of file data if resident, otherwise size of data run table.
	bool resident; // If resident, offset is to raw data, if non resident, offset is to data runs
	bool is_directory;
	bool valid;
	bool ready;
} Disk_Entry;

typedef struct Volume_Database {
	string name; // C, D, E etc
	
	bool done; // Is this done or still indexing ?
	
	Disk_Entry *entries;
	u64 entry_count;
	
	Disk_Entry **entries_per_record;
	
	Disk_Entry *root_entry;
	
	u64 file_record_hole_count;
	u64 corrupt_usn_count;
	u64 compressed_count;
	u64 attr_list_count;
	u64 unnamed_count;
	u64 dataless_count;
	
	f64 waiting_for_io_time;
	u64 bytes_read;
	
} Volume_Database;

typedef struct Work {
	u8* data;
	u64 size;
} Work;

typedef struct Query_Context {
	u8 *read_buffer; // Circular
	u64 next_buffer_index;
	u64 buffered;
	
	Work *work; // Persistent array
	u64 next_work_index;
	
	struct Query *query;
	Semaphore found_entry_sem;
	u64 read_entry_count;
	
	string volume_name;
	
} Query_Context;

typedef struct Query {
	Volume_Database vol;
	Disk_Entry **child_entries; // persistent array
	Disk_Entry **filtered_entries; // persistent array
	Query_Context ctx;
} Query;

typedef struct Record_Range {
	u64 first_record_index;
	u64 record_count;
} Record_Range;

typedef struct Timestamp {
	string label;
	f64 t0;
	
	f64 accum;
	u64 count;
} Timestamp;

Timestamp timestamps[1024] = {0};
u64 timestamp_count;

void timestamp_begin(string label) {
	Timestamp *ts = 0;
	
	for (u64 i = 0; i < timestamp_count; i += 1) {
		if (strings_match(timestamps[i].label, label)) {
			ts = &timestamps[i];
			break;
		}
	}
	
	if (!ts) {
		ts = &timestamps[timestamp_count++];
		*ts = (Timestamp){0};
		ts->label = label;
	}
	
	ts->t0 = sys_get_seconds_monotonic();
}

void timestamp_end(string label) {
	f64 t1 = sys_get_seconds_monotonic();
	
	Timestamp *ts = 0;
	
	for (u64 i = 0; i < timestamp_count; i += 1) {
		if (strings_match(timestamps[i].label, label)) {
			ts = &timestamps[i];
			break;
		}
	}
	
	if (ts) {
		f64 elapsed = t1 - ts->t0;
		
		ts->accum += elapsed;
		ts->count += 1;
	}
}

#define BYTES_PER_READBACK (MiB(32))
unit_local u64 max_streaming_memory = GiB(3);
unit_local bool is_entire_volume_search = false;
unit_local bool is_command_line = false;

bool index_volume(string disk_id, Volume_Database *vol) {
	
	*vol = (Volume_Database){0};
	
	vol->name = disk_id;
	
	string disk_file_name = tprint("\\\\.\\%s:", disk_id);
	
	u16 _cpath[MAX_PATH_LENGTH*2];
	u16 *cpath = _cpath;
	_win_utf8_to_wide(disk_file_name, cpath, MAX_PATH_LENGTH*2);
	File_Handle f = CreateFileW(
		cpath, GENERIC_READ,
		FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
		0, OPEN_EXISTING, FILE_FLAG_RANDOM_ACCESS | FILE_FLAG_NO_BUFFERING, 0
	);
	File_Handle fasync = CreateFileW(
		cpath, GENERIC_READ,
		FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
		0, OPEN_EXISTING, FILE_FLAG_RANDOM_ACCESS | FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED, 0
	);
	
	if (f == (File_Handle)0xffffffffffffffff) {
		print("Could not open volume for reading.\n");
		return false;
	}
	if (fasync == (File_Handle)0xffffffffffffffff) {
		print("Could not open volume for async reading.\n");
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
	
	u64 max_readback_memory = max(max_streaming_memory, BYTES_PER_READBACK);
	u64 max_readback_count = max_readback_memory/BYTES_PER_READBACK;
	
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
		memset(vol->entries, 0, entries_mem_size);
	}
	
	////////////
	/// Walk file records. Read future file records from disk asynchronously.
	
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
		
		mv.QuadPart = (LONGLONG)(range.first_record_index*record_size);
		SetFilePointerEx(f, mv, &new_pointer, 0/*FILE_BEGIN*/);
		
		u64 records_bytes_read = 0;
		u64 records_bytes_queued = 0;
		
		while (records_bytes_read < range.record_count*record_size) tm_scope("Record Range step") {
			
			u8 *readback_data = 0; (void)readback_data;
			
			/////
			// Queue a maximum of 'max_queues_per_iter' future reads
			// (Doing them all up front is slower for some reason ?)
			
			u64 max_queues_per_iter = 3;
			u64 queued_this_iter = 0;
			
			while (records_bytes_queued < range.record_count*record_size && queued_this_iter < max_queues_per_iter) tm_scope("Queue read") {
				
				if (readback_count >= max_readback_count) {
					break;
				}
				
				u64 remainder = (range.record_count*record_size) - records_bytes_queued;
				u64 to_read_async = (DWORD)min(remainder, BYTES_PER_READBACK);
				
				if (to_read_async) tm_scope("Queue next ReadFile") {
					u8 *target_data = records_buffer + (readback_place_position%max_readback_count)*BYTES_PER_READBACK;
					
					OVERLAPPED *ovl = ovl_buffer + (readback_place_position % max_readback_count);
					*ovl = (OVERLAPPED){0};
					u64 byte_offset = range.first_record_index*record_size + records_bytes_queued;
					ovl->Offset = byte_offset & 0xFFFFFFFF;
					ovl->OffsetHigh = byte_offset >> 32;
					ovl->hEvent = events[(readback_place_position % max_readback_count)];
					ResetEvent(events[(readback_place_position % max_readback_count)]);
					tm_scope("Async ReadFile call")
					ReadFile(fasync, target_data, (DWORD)to_read_async, 0, ovl);
					vol->bytes_read += to_read_async;
					
					read_sizes[(readback_place_position % max_readback_count)] = to_read_async;
					
					sys_atomic_add_64(&readback_count, 1);
					readback_place_position += 1;
					
					records_bytes_queued += to_read_async;
					
					queued_this_iter += 1;
					
				} else continue;
			}
			
			if (records_bytes_read >= range.record_count*record_size) break;
			
			
			/////
			// Wait for next async read to finish
			
			u64 record_offset = 0;
			
			tm_scope("Wait Last ReadFile") {
				OVERLAPPED *ovl = ovl_buffer + (readback_next_position % max_readback_count);
				
				DWORD async_read = 0;
				f64 io0 = sys_get_seconds_monotonic();
				BOOL ok = GetOverlappedResult(fasync, ovl, &async_read, true);
				f64 io1 = sys_get_seconds_monotonic();
				vol->waiting_for_io_time += io1-io0;
				assert(ok);
				assert(read_sizes[(readback_next_position % max_readback_count)] == async_read);
				
				readback_data = records_buffer + (readback_next_position%max_readback_count)*BYTES_PER_READBACK;
				
				readback_next_position += 1;
				sys_atomic_add_64(&readback_count, (u64)-1);
				
				record_offset = (u64)ovl->Offset | ((u64)ovl->OffsetHigh << 32);
				
				records_bytes_read += async_read;
				
				read = async_read;
			}
			
			/////
			// Process the records that were fetched
			
			u64 to_process = read/record_size;
			
			tm_scope("Process streamed records")
			for (u64 j = 0; j < to_process; j += 1) {
				
				Disk_Entry *next_entry = vol->entries + (processed_record_count + j);
				next_entry->valid = true;
				
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
				
				/////
				// Walk attributes
				
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
						} else {
							next_entry->valid = false;
						}
						
						if (strings_match(name, STR("."))) {
							vol->root_entry = next_entry;
						}
						
						
					} else if (attr_header_base->type == ATTR_DATA && !next_entry->is_directory) {
						next_entry->resident = resident;
						
						if (resident) {
							Attribute_Header_Resident *attr_header = (Attribute_Header_Resident *)attr_header_base;
							next_entry->size = attr_header->attribute_length;
							next_entry->file_position = record_offset + (u64)attr_header-(u64)rec + attr_header->attribute_offset;
						} else {
							Attribute_Header_Non_Resident *attr_header = (Attribute_Header_Non_Resident *)attr_header_base;
							next_entry->size = ((u64)attr_header->base.length_including_header - (u64)attr_header->offset_to_data_runs);
							next_entry->file_position = record_offset + (u64)attr_header-(u64)rec + attr_header->offset_to_data_runs;
						}
						
					}
					
					// This attribute is not like other attributes so it will break stuff
					if (attr_header_base->type == ATTR_LOGGED_UTILITY_STREAM) {
						break;
					}
					
					// @todo Walk attributes in attribute list 
					if (attr_header_base->type == ATTR_ATTRIBUTE_LIST) {
						has_attr_list = true;
					}
					
					attr_header_base = (Attribute_Header_Base*)((u8*)attr_header_base + attr_header_base->length_including_header);
				}
				
				if (!(rec->flags & FILE_FLAGS_Compressed)) {
					vol->entries_per_record[processed_record_count+j] = next_entry;
					
					if (!next_entry->is_directory && !string_starts_with(next_entry->name, STR("$")) && !has_attr_list && has_name) {
						if (next_entry->file_position == 0) {
							vol->dataless_count += 1;
						}
					}
					
					next_entry->ready = true;
				}
				
				if (rec->flags & FILE_FLAGS_Compressed) {
					vol->compressed_count += 1;
				}
				if (has_attr_list) {
					vol->attr_list_count += 1;
				}
				if (!has_name) {
					vol->unnamed_count += 1;
					next_entry->valid = false;
				}
			}
			
			processed_record_count += to_process;
		}
	}
	
	if (!is_command_line) {
		for (u64 k = 0; k < max_readback_count; k += 1) CloseHandle(events[k]);
		sys_unmap_pages(records_buffer);
		CloseHandle(f);
		CloseHandle(fasync);
	}
	
	vol->done = true;
	return true;
}

/*void update_volume_blocking(Volume_Database *state, bool *keep_going_flag) {
	while (*keep_going_flag) {
		
	}
}*/

string tprint_entry_full_path(string volume_label, Disk_Entry *entry) {
	string dirpath = STR("");
	
	if (entry->pointer_to_parent_entry) {
		Disk_Entry *parent = *entry->pointer_to_parent_entry;
		while (parent && parent != *parent->pointer_to_parent_entry && !strings_match(parent->name, STR("."))) {
			dirpath = tprint("%s\\%s", parent->name, dirpath);
			
			if (parent->pointer_to_parent_entry) {
				parent = *parent->pointer_to_parent_entry;
			} else break;
		}
	}
	
	dirpath = tprint("%s:\\%s", volume_label, dirpath);
	
	return tprint("%s%s", dirpath, entry->name);
}

typedef enum Task_Kind {
	TASK_INDEX_VOLUME,
	TASK_QUERY_VOLUME,
} Task_Kind;

typedef struct Task_Pipe {
	f64 duration_seconds;
	bool done;
	bool success;
	bool partial;
} Task_Pipe;

typedef struct Task {
	Task_Kind kind;
	
	// Indexing
	string volume_name;
	Volume_Database *vol; // Also for querying
	
	// Querying
	string search_directory;
	string *filters;
	u64 filter_count;
	Query *query;
	
	Task_Pipe *result;
} Task;

typedef struct Volume_State {
	Volume_Database vol;
	Task_Pipe indexing_pipe;
	Task_Pipe query_pipe;
} Volume_State;

typedef struct Gui_State {
	Volume_State *vol_states;
	u64 vol_count;
} Gui_State;

Gui_State state = (Gui_State){0};

int gui_loop(void);

Disk_Entry *full_path_to_entry_directory(Volume_Database vol, string path) {
	timestamp_begin(STR("Find directory entry"));
	
	u64 slashes = string_count_occurences(path, STR("\\"));
	Disk_Entry *path_entry = 0;
	
	if (slashes > 0) {
		u64 search_dir_name_count = 0;
		string *search_dir_names = PushTempBuffer(string, slashes+1);
		
		string next = path;
		for (u64 i = 0; i < slashes; i += 1) {
			s64 slash_index = string_find_index_from_right(next, STR("\\"));
			search_dir_names[search_dir_name_count++] = string_slice(next, (u64)slash_index+1, next.count-(u64)slash_index-1);
			next = string_slice(next, 0, (u64)slash_index);
		}
		search_dir_names[search_dir_name_count++] = STR(".");
		
		for (u64 i = 0; i < vol.entry_count; i += 1) {
			
			u64 dir_match_count = 0;
			
			Disk_Entry *entry = &vol.entries[i];
			
			if (!entry->is_directory) continue;
			
			Disk_Entry *next_dir = entry;
			for (u64 j = 0; j < search_dir_name_count; j += 1) {
				
				if (strings_match(next_dir->name, search_dir_names[j])) {
					dir_match_count += 1;
				} else {
					dir_match_count = 0;
				}
				
				if (strings_match(next_dir->name, STR("."))) {
					break;
				}
				
				if (!next_dir->pointer_to_parent_entry) {
					break;
				}
				next_dir = *next_dir->pointer_to_parent_entry;
			}
			
			if (dir_match_count == search_dir_name_count) {
				path_entry = entry;
				break;
			}
		}
		
	} else {
		path_entry = vol.root_entry;
	}
	
	timestamp_end(STR("Find directory entry"));
	
	return path_entry;
}

s64 stream_thread_proc(Thread *t) {
	Query_Context *ctx = (Query_Context*)t->userdata;
	
	u16 _cpath[MAX_PATH_LENGTH*2];
	u16 *cpath = _cpath;
	_win_utf8_to_wide(ctx->volume_name, cpath, MAX_PATH_LENGTH*2);
	File_Handle f = CreateFileW(
		cpath, GENERIC_READ,
		FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
		0, OPEN_EXISTING, FILE_FLAG_RANDOM_ACCESS | FILE_FLAG_NO_BUFFERING, 0
	);
	
	if (f == (File_Handle)0xffffffffffffffff) {
		print("stream_thread_proc: Could not open volume for reading.\n");
		return 1;
	}
	
	while (1) {
		reset_temporary_storage();
		
		sys_semaphore_wait(ctx->found_entry_sem);
		
		if (ctx->read_entry_count >= persistent_array_count(ctx->query->filtered_entries)) {
			break;
		}
		
		Disk_Entry *entry = ctx->query->filtered_entries[ctx->read_entry_count++];
		
		if (entry->resident) {
			LARGE_INTEGER mv;
			mv.QuadPart = (LONGLONG)entry->file_position;
			LARGE_INTEGER new_pointer;
			SetFilePointerEx(f, mv, &new_pointer, 0/*FILE_BEGIN*/);
			
			u8 *data = PushTempBuffer(u8, entry->size);
			
			string content = (string) {entry->size, data};
			
			print("%s:\n=====================\n%s\n=====================\n\n", entry->name, content);
		}
	}
	
	return 0;
}

Query query_volume(Volume_Database vol, Disk_Entry *directory, string *filters, u64 filter_count) {
	Query query = (Query){0};
	query.vol = vol;
	
	query.ctx = (Query_Context){0};
	{
		u64 ps = sys_get_info().page_size;
		u64 page_count = (max_streaming_memory+ps)/ps;
		query.ctx.read_buffer = sys_map_pages(SYS_MEMORY_RESERVE | SYS_MEMORY_ALLOCATE, 0, page_count, false);
		
		persistent_array_init((void**)&query.ctx.work, sizeof(Work));
		
		query.ctx.query = &query;
		
		sys_semaphore_init(&query.ctx.found_entry_sem);
		
		query.ctx.volume_name = vol.name;
	}
	
	persistent_array_init((void**)&query.child_entries, sizeof(Disk_Entry*));
	persistent_array_init((void**)&query.filtered_entries, sizeof(Disk_Entry*));
	
	timestamp_begin(STR("Find children of search directory"));
	for (u64 i = 0; i < vol.entry_count; i += 1) {
		Disk_Entry *entry = &vol.entries[i];
		
		if (!entry->valid)          continue;
		if (!entry->ready)          continue;
		if (entry->is_directory)    continue;
		if (entry->name.count == 0) continue;
		
		if (entry->pointer_to_parent_entry) {
			bool search_dir_is_ancestor = false;
			
			Disk_Entry *next = entry;
			
			while (next->valid && *next->pointer_to_parent_entry && *next->pointer_to_parent_entry != next) {
				if (*next->pointer_to_parent_entry == directory) {
					search_dir_is_ancestor = true;
					break;
				}
				next = *next->pointer_to_parent_entry;
			}
			
			if (!search_dir_is_ancestor) continue;
			
			persistent_array_push_copy(query.child_entries, &entry);
		}
	}
	timestamp_end(STR("Find children of search directory"));
	
	timestamp_begin(STR("Find children which match filter"));
	for (u64 i = 0; i < persistent_array_count(query.child_entries); i += 1) {
		Disk_Entry *entry = query.child_entries[i];
		
		bool any_match = false;
		
		string name = entry->name;
		
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
		
		if (any_match) {
			persistent_array_push_copy(query.filtered_entries, &entry);
			sys_semaphore_signal(&query.ctx.found_entry_sem);
		}
	}
	timestamp_end(STR("Find children which match filter"));
	
	return query;
}

void query_free(Query query) {
	sys_unmap_pages(query.ctx.read_buffer);
	persistent_array_uninit(query.ctx.work);
	sys_semaphore_release(query.ctx.found_entry_sem);
	persistent_array_uninit(query.child_entries);
	persistent_array_uninit(query.filtered_entries);
}

u64 parse_filters(string full_filter, string *filters, u64 max_count) {
	u64 filter_count = 0;
	
	s64 index;
	while ((index = string_find_index_from_left(full_filter, STR(","))) != -1) {
		string filter = string_trim(string_slice(full_filter, 0, (u64)index));
		if (filter.count > 0) filters[filter_count++] = filter;
		full_filter = string_slice(full_filter, (u64)index+1, full_filter.count-(u64)index-1);
		
		if (filter_count == max_count) break;
	}
	if (filter_count != max_count) {
		full_filter = string_trim(full_filter);
		if (full_filter.count > 0) filters[filter_count++] = full_filter;
	}
	
	return filter_count;
}

void print_stats(Volume_Database vol) {
	print("file_record_hole_count: %u\n", vol.file_record_hole_count);
	print("corrupt_usn_count: %u\n", vol.corrupt_usn_count);
	print("compressed_count: %u\n", vol.compressed_count);
	print("attr_list_count: %u\n", vol.attr_list_count);
	print("unnamed_count: %u\n", vol.unnamed_count);
	print("dataless_count: %u\n", vol.dataless_count);
	f64 ms = vol.waiting_for_io_time * 1024;
	print("Time idle waiting for disk: %f ms\n", ms);
	f64 mb = (f64)vol.bytes_read/1024.0/1024.0;
	print("File record bytes read: %f MB\n", mb);
	print("Total file records processed: %u\n", vol.entry_count);
	for (u64 i = 0; i < timestamp_count; i += 1) {
		Timestamp *ts = &timestamps[i];
		ms = ts->accum * 1000.0;
		
		print("\t%s: %f ms\n", ts->label, ms);
	}
}

int main(int argc, char **argv) {
	string search_directory = STR("");
	bool has_search_directory = false;
	string search_term = STR("*"); (void)search_term;
	bool has_search_term = false; (void)has_search_term;
	string filters[2048] = {0};
	u64 filter_count = 0;
	
	if (argc == 1) {
		gui_loop();
		print("Goodbye, Gui.\n");
	} else {
		
		is_command_line = true;
		
		/////////////////
		/// Parse command line
		
		{
			timestamp_begin(STR("Parse Command Line"));
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
			
			filter_count = parse_filters(full_filter, filters, 2048);
			
			if (!sys_get_absolute_path(get_temp(), search_directory, &search_directory)) {
				print("Invalid path '%s'\n", search_directory);
				sys_exit(1);
			}
			
			timestamp_end(STR("Parse Command Line"));
		}
		
		timestamp_begin(STR("Index Volume"));
		
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
		
		Volume_Database vol;
		
		bool vol_ok = index_volume(disk_id, &vol);
		
		if (!vol_ok) {
			print("Indexing volume failed.\n");
			sys_exit(1);
		}
		
		timestamp_end(STR("Index Volume"));
		
		Disk_Entry *search_directory_entry = full_path_to_entry_directory(vol, search_directory);
		
		if (!search_directory_entry) {
			print("Could not find search directory entry\n");
		}
		
		Query query = query_volume(vol, search_directory_entry, filters, filter_count);
		
		for (u64 i = 0; i < persistent_array_count(query.filtered_entries); i += 1) {
			string path = tprint_entry_full_path(disk_id, query.filtered_entries[i]);
			print("%s\n", path);
		}
		
		print_stats(vol);
		
		
		print("Goodbye, Terminal.\n");
	}
	
	dump_profiler_result();
	
	return 0;
}




int gui_callback(struct nk_context *ctx)
{
	static char dir[MAX_PATH]         = {0};
	static char filter[MAX_FILTER]    = {0};
	static char search_str[MAX_SEARCH]= {0};
	static Query queries[1024];
	static u64 query_count = 0;
	static s64   selected     = -1;
	static struct nk_scroll scroll = {0};
	
	nk_layout_row_dynamic(ctx, 30, 1);
	nk_label(ctx, "Directory:", NK_TEXT_LEFT);
	nk_edit_string_zero_terminated(ctx, NK_EDIT_SIMPLE,
		dir, MAX_PATH, nk_filter_default);
	
	nk_layout_row_dynamic(ctx, 30, 1);
	nk_label(ctx, "Filter:", NK_TEXT_LEFT);
	nk_edit_string_zero_terminated(ctx, NK_EDIT_SIMPLE,
		filter, MAX_FILTER, nk_filter_default);
	
	nk_layout_row_dynamic(ctx, 30, 1);
	nk_label(ctx, "Search substring:", NK_TEXT_LEFT);
	nk_edit_string_zero_terminated(ctx, NK_EDIT_SIMPLE,
		search_str, MAX_SEARCH, nk_filter_default);
	
	nk_layout_row_dynamic(ctx, 30, 2);
	if (nk_button_label(ctx, "Search")) {
		
		for (u64 i = 0; i < query_count; i += 1) {
			query_free(queries[i]);
		}
		query_count = 0;
		
		string dirs = STR(dir);
		sys_get_absolute_path(get_temp(), dirs, &dirs);
		
		s64 dir_colon_index = string_find_index_from_left(dirs, STR(":"));
		
		bool one_volume = dir_colon_index != -1;
		
		for (u64 i = 0; i < state.vol_count; i += 1) {
			Volume_Database vol = state.vol_states[i].vol;
			
			if (one_volume) {
				
				if (strings_match(string_slice(dirs, 0, (u64)dir_colon_index), vol.name)) {
				
					Disk_Entry *search_directory_entry = full_path_to_entry_directory(vol, dirs);
					if (!search_directory_entry) {
						print("Could not find search directory entry\n");
						continue;
					}
					
					string filters[2048];
					u64 filter_count = parse_filters(STR(filter), filters, 2048);
					queries[query_count++] = query_volume(vol, search_directory_entry, filters, filter_count);
					
					print_stats(vol);
					timestamp_count = 0;
					
					break;
				}
				
			}
		}
		
		
		selected = -1;
	}
	if (nk_button_label(ctx, "Clear")) {
		for (u64 i = 0; i < query_count; i += 1) {
			query_free(queries[i]);
		}
		query_count = 0;
		selected     = -1;
	}
	
	struct nk_rect region = nk_window_get_content_region(ctx);
	struct nk_rect last = nk_widget_bounds(ctx);
	float used = (last.y + last.h) - region.y;
	float remaining = region.h - used;
	
	for (u64 i = 0; i < state.vol_count; i += 1) {
		Volume_State *vstate = state.vol_states + i;
		
		if (vstate->vol.done) {
			string text = tprint("%s: Done (%f seconds)", vstate->vol.name, vstate->indexing_pipe.duration_seconds);
			nk_text(ctx, (char*)text.data, (int)text.count, NK_TEXT_LEFT);
		} else if (!vstate->indexing_pipe.done) {
			string text = tprint("%s: Indexing ...", vstate->vol.name);
			nk_text(ctx, (char*)text.data, (int)text.count, NK_TEXT_LEFT);
		} else {
			string text = tprint("%s: Error", vstate->vol.name);
			nk_text(ctx, (char*)text.data, (int)text.count, NK_TEXT_LEFT);
		}
	}
	
	nk_layout_row_dynamic(ctx, remaining, 1);
	if (nk_group_scrolled_begin(ctx,
		&scroll,
		"Results",
		NK_WINDOW_BORDER))
	{
		nk_layout_row_dynamic(ctx, 20, 2);
		
		u64 index = 0;
		for (u64 i = 0; i < query_count; ++i) {
			for (u64 j = 0; j < persistent_array_count(queries[i].filtered_entries); ++j) {
				string name = tprint_entry_full_path(queries[i].vol.name, queries[i].filtered_entries[j]);
				nk_bool is_sel = (selected == (s64)index);
				if (nk_selectable_text(ctx, (char*)name.data, (int)name.count, NK_TEXT_LEFT, &is_sel)) {
					selected = (s64)index;
				}
				nk_text(ctx, "<content>", (int)c_style_strlen("<content>"), NK_TEXT_LEFT);
				index++;
			}
		}
		nk_group_scrolled_end(ctx);
	}
	
	return 1;
}

unit_local bool gui_running = true;
unit_local Task task_buffer[1024]; // Circular buffer
unit_local u64 next_task_work_index = 0;
unit_local u64 next_task_submit_index = 0;
unit_local u64 task_count = 0;
unit_local Semaphore task_sem;

s64 task_thread(Thread *t) {
	(void)t;
	
	while (gui_running) {
		
		sys_semaphore_wait(task_sem);
		
		if (!gui_running) break;
		
		assert(task_count > 0);
		sys_atomic_add_64(&task_count, (u64)-1);
		assert(task_count < 1024);
		
		u64 task_index = sys_atomic_add_64(&next_task_work_index, 1) % 1024;
		
		Task *task = &task_buffer[task_index];
		*task->result = (Task_Pipe){0};
		
		f64 t0 = sys_get_seconds_monotonic();
		
		switch (task->kind) {
			case TASK_INDEX_VOLUME: {
				task->result->success = index_volume(task->volume_name, task->vol);
				break;
			}
			case TASK_QUERY_VOLUME: {
				task->result->partial = !task->vol->done;
				Disk_Entry *search_directory_entry = full_path_to_entry_directory(*task->vol, task->search_directory);
				*task->query = query_volume(*task->vol, search_directory_entry, task->filters, task->filter_count);
				task->result->success = true;
				break;
			}
			
			default: assert(false); break;
		}
		f64 t1 = sys_get_seconds_monotonic();
		
		task->result->duration_seconds = t1-t0;
		task->result->done = true;
	}
	
	return 0;
}

int gui_loop(void) {
	
	sys_semaphore_init(&task_sem);
	
	System_Info sys_info = sys_get_info();
	
	Arena arena = make_arena(GiB(8), KiB(32));
	
	
	u64 thread_count = (u64)max(((f64)sys_info.logical_cpu_count * 0.75), 1);
	Thread *task_threads = (Thread*)arena_push(&arena, sizeof(Thread)*thread_count);
	
	for (u64 i = 0; i < thread_count; i += 1) {
		Thread *thread = task_threads + i;
		sys_thread_init(thread, task_thread, 0);
		sys_thread_start(thread);
	}
	
	state.vol_states = (Volume_State*)arena_push(&arena, sizeof(Volume_State)*256);
	
	u64 names_size = (u64)GetLogicalDriveStringsA(0, 0);
	u8 *names_buffer = (u8*)arena_push(&arena, names_size);
	
	GetLogicalDriveStringsA((DWORD)names_size, (LPSTR)names_buffer);
	u8 *next = names_buffer;
	
	while (next < names_buffer + names_size) {
		string str = STR(next);
		
		s64 colon_index = string_find_index_from_left(str, STR(":"));
		if (colon_index > 0) {
			
			string volume_name = string_slice(str, 0, (u64)colon_index);
			Volume_State *vstate = state.vol_states + (state.vol_count++);
			*vstate = (Volume_State){0};
			
			u64 index = sys_atomic_add_64(&next_task_submit_index, 1) % 1024;
			
			Task *task = &task_buffer[index];
			*task = (Task){0};
			task->vol = &vstate->vol;
			task->volume_name = volume_name;
			task->kind = TASK_INDEX_VOLUME;
			
			task->result = &vstate->indexing_pipe;
			
			sys_atomic_add_64(&task_count, 1);
			assert(task_count <= 1024);
			
			sys_semaphore_signal(&task_sem);
			
			
			/*print("Indexing volume '%s'\n", volume_name);
			bool ok = index_volume(volume_name, &state.vols[state.vol_count++]);
			if (!ok) {
				print("Failed indexing volume '%s'\n", volume_name);
				state.vol_count -= 1;
			} else {
				print("Indexed volume '%s'\n", volume_name);
			}*/
			
			
		}
		
		next += str.count+1;
	}
	
	nkgdi_window_init();
	
	struct nkgdi_window w1;
	memset(&w1, 0x0, sizeof(struct nkgdi_window));
	
	w1.allow_sizing = 1;
	w1.allow_maximize = 1;
	w1.allow_move = 1;
	w1.has_titlebar = 1;
	w1.cb_on_draw = &gui_callback;
	nkgdi_window_create(&w1, 1024, 1024, "nowgrep", 520, 10);
	
	f64 target_ms = 1.0/60.0;
	
	f64 last_time = sys_get_seconds_monotonic();
	
	while (nkgdi_window_update(&w1) && gui_running) {
		reset_temporary_storage();
		f64 now = sys_get_seconds_monotonic();
		f64 delta = now - last_time;
		
		if (delta < target_ms) {
			DWORD sleep_ms = (DWORD)((target_ms - delta) * 1000.0);
			if (sleep_ms > 0) Sleep(sleep_ms);
			
			while ((now = sys_get_seconds_monotonic()) - last_time < target_ms) {
				/* spin */
			}
		}
		
		last_time = sys_get_seconds_monotonic();
	}
	
	gui_running = false;
	
	nkgdi_window_destroy(&w1);
	
	nkgdi_window_shutdown();
	
	print("Waiting for task threads to finish...\n");
	for (u64 i = 0; i < thread_count; i += 1) {
		sys_semaphore_signal(&task_sem);
	}
	for (u64 i = 0; i < thread_count; i += 1) {
		Thread *thread = task_threads + i;
		sys_thread_join(thread);
		sys_thread_close(thread);
	}
	print("All task threads are done.\n");
	
	return 0;
}
