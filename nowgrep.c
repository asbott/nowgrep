
#define OSTD_IMPL
#include "One-Std/one-headers/one_system.h"
#include "One-Std/one-headers/one_print.h"
#include "One-Std/one-headers/one_math.h"
#include "One-Std/one-headers/one_unicode.h"
#include "One-Std/one-headers/one_path_utils.h"

#include "temporary_nonsense_for_nuklear.h"

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

//#define ENABLE_PROFILING

f64 seconds_since_init(void) {
	local_persist f64 start = 0.0;
	local_persist bool initted = false;
	
	if (!initted) {
		initted = true;
		start = sys_get_seconds_monotonic();
	}
	
	return sys_get_seconds_monotonic() - start;
}

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
	ATTR_REPARSE_POINT = 0xC0,
	
	ATTR_LOGGED_UTILITY_STREAM = 0x100,
	
	ATTR_HANDLED = (int)0xDEADBEEF,
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

typedef enum Windows_Reparse_Tag {
	IO_REPARSE_TAG_MOUNT_POINT      = (s32)0xA0000003L,
	IO_REPARSE_TAG_HSM              = (s32)0xC0000004L,
	IO_REPARSE_TAG_HSM2             = (s32)0x80000006L,
	IO_REPARSE_TAG_SIS              = (s32)0x80000007L,
	IO_REPARSE_TAG_WIM              = (s32)0x80000008L,
	IO_REPARSE_TAG_CSV              = (s32)0x80000009L,
	IO_REPARSE_TAG_DFS              = (s32)0x8000000AL,
	IO_REPARSE_TAG_SYMLINK          = (s32)0xA000000CL,
	IO_REPARSE_TAG_DFSR             = (s32)0x80000012L,
	IO_REPARSE_TAG_DEDUP            = (s32)0x80000013L,
	IO_REPARSE_TAG_NFS              = (s32)0x80000014L,
	IO_REPARSE_TAG_FILE_PLACEHOLDER = (s32)0x80000015L,
	IO_REPARSE_TAG_WOF              = (s32)0x80000017L,
	IO_REPARSE_TAG_WCI              = (s32)0x80000018L,
	IO_REPARSE_TAG_WCI_1            = (s32)0x90001018L,
	IO_REPARSE_TAG_GLOBAL_REPARSE   = (s32)0xA0000019L,
	IO_REPARSE_TAG_CLOUD            = (s32)0x9000001AL,
	IO_REPARSE_TAG_CLOUD_1          = (s32)0x9000101AL,
	IO_REPARSE_TAG_CLOUD_2          = (s32)0x9000201AL,
	IO_REPARSE_TAG_CLOUD_3          = (s32)0x9000301AL,
	IO_REPARSE_TAG_CLOUD_4          = (s32)0x9000401AL,
	IO_REPARSE_TAG_CLOUD_5          = (s32)0x9000501AL,
	IO_REPARSE_TAG_CLOUD_6          = (s32)0x9000601AL,
	IO_REPARSE_TAG_CLOUD_7          = (s32)0x9000701AL,
	IO_REPARSE_TAG_CLOUD_8          = (s32)0x9000801AL,
	IO_REPARSE_TAG_CLOUD_9          = (s32)0x9000901AL,
	IO_REPARSE_TAG_CLOUD_A          = (s32)0x9000A01AL,
	IO_REPARSE_TAG_CLOUD_B          = (s32)0x9000B01AL,
	IO_REPARSE_TAG_CLOUD_C          = (s32)0x9000C01AL,
	IO_REPARSE_TAG_CLOUD_D          = (s32)0x9000D01AL,
	IO_REPARSE_TAG_CLOUD_E          = (s32)0x9000E01AL,
	IO_REPARSE_TAG_CLOUD_F          = (s32)0x9000F01AL,
	IO_REPARSE_TAG_CLOUD_MASK       = (s32)0x0000F000L,
	IO_REPARSE_TAG_APPEXECLINK      = (s32)0x8000001BL,
	IO_REPARSE_TAG_PROJFS           = (s32)0x9000001CL,
	IO_REPARSE_TAG_STORAGE_SYNC     = (s32)0x8000001EL,
	IO_REPARSE_TAG_WCI_TOMBSTONE    = (s32)0xA000001FL,
	IO_REPARSE_TAG_UNHANDLED        = (s32)0x80000020L,
	IO_REPARSE_TAG_ONEDRIVE         = (s32)0x80000021L,
	IO_REPARSE_TAG_PROJFS_TOMBSTONE = (s32)0xA0000022L,
	IO_REPARSE_TAG_AF_UNIX          = (s32)0x80000023L,
} Windows_Reparse_Tag;

#define REPARSE_Is_alias (0x20000000)
#define REPARSE_Is_High_Latency (0x40000000)
#define REPARSE_Is_Microsoft (0x80000000)
#define REPARSE_NSS (0x68000005)
#define REPARSE_NSS_Recover (0x68000006)
#define REPARSE_SIS (0x68000007)
#define REPARSE_DFS (0x68000008)
#define REPARSE_Mount_Point (0x88000003)
#define REPARSE_HSM (0xA8000004)
#define REPARSE_Symbolic_Link (0xE8000000)

typedef struct Reparse_Point {
	u32 flags;
	u16 length;
	u16 padding;
	u8 data_and_maybe_third_party_guid[]; // If not microsoft, it's "third party"
} Reparse_Point;

typedef struct Reparse_Data_Symbolic_Link {
	u16 name_offset;
	u16 name_length;
	u16 print_name_offset;
	u16 print_name_length;
	u16 path_buffer;
} Reparse_Data_Symbolic_Link;

typedef struct Attribute_List_Record {
	Attribute_Type type;
	u16 record_length;
	u8 name_length;
	u8 name_offset;
	u64 starting_vcn;
	u64 base_file_reference_of_attribute;
	u16 attribute_id;
	u16 name[];
} Attribute_List_Record;

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
	u64 base_file_record_reference; // This is only non-zero if this is a dummy file record for holding ATTRIBUTE_LIST entries
	u16 next_attribute_id;
	u16 pad;
	u32 mft_record_number;
	u16 update_sequence_number;
	u8 update_sequence[];
} File_Record;

#pragma pack(pop)

// https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/ns-ntifs-_reparse_data_buffer
typedef struct REPARSE_DATA_BUFFER {
	ULONG  ReparseTag;
	USHORT ReparseDataLength;
	USHORT Reserved;
	union {
		struct {
			USHORT SubstituteNameOffset;
			USHORT SubstituteNameLength;
			USHORT PrintNameOffset;
			USHORT PrintNameLength;
			ULONG  Flags;
			WCHAR  PathBuffer[1];
		} SymbolicLinkReparseBuffer;
		struct {
			USHORT SubstituteNameOffset;
			USHORT SubstituteNameLength;
			USHORT PrintNameOffset;
			USHORT PrintNameLength;
			WCHAR  PathBuffer[1];
		} MountPointReparseBuffer;
		struct {
			UCHAR DataBuffer[1];
		} GenericReparseBuffer;
	} DUMMYUNIONNAME;
} REPARSE_DATA_BUFFER;

// https://learn.microsoft.com/en-us/windows/win32/api/fileapi/ns-fileapi-by_handle_file_information
typedef struct BY_HANDLE_FILE_INFORMATION {
  DWORD    dwFileAttributes;
  FILETIME ftCreationTime;
  FILETIME ftLastAccessTime;
  FILETIME ftLastWriteTime;
  DWORD    dwVolumeSerialNumber;
  DWORD    nFileSizeHigh;
  DWORD    nFileSizeLow;
  DWORD    nNumberOfLinks;
  DWORD    nFileIndexHigh;
  DWORD    nFileIndexLow;
} BY_HANDLE_FILE_INFORMATION, *PBY_HANDLE_FILE_INFORMATION, *LPBY_HANDLE_FILE_INFORMATION;
// https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getfileinformationbyhandle
BOOL GetFileInformationByHandle(HANDLE hFile, LPBY_HANDLE_FILE_INFORMATION lpFileInformation);

// @todo @memory compress this as much as possible
// - u32 entry indices instead of pointers
// - u16 string length? Make name u32 offset into string pool, u16 length (u48 total, currently u128)
// - Flags
typedef struct Entry {
		
	string name;
	u8 *data; // If resident: file contents, otherwise data runs.
	u64 size; // Size of file data
	
	struct Entry *parent;
	struct Entry *next_sibling;
	struct Entry *first_child;
	
	// Mike acton would cry (@todo flags)
	bool resident; // If resident, offset is to raw data, if non resident, offset is to data runs
	bool is_directory;
	bool valid;
	bool ready;
	bool was_searched;
	bool usa_applied;
	bool is_symlink; // If symlink, first_child is set to the entry being linked to, and all other members are ignored
} Entry;

typedef struct Volume_Database {
	volatile Ntfs_Boot_Sector boot;
	
	string name; // C, D, E etc
	
	bool done; // Is this done or still indexing ?
	
	Entry *entries;
	u64 entry_count;
	
	Arena entry_names_arena; // Small file contents & data runs ++ entry name strings
	Arena entry_small_data_arena;
	Arena entry_data_runs_arena;
	Arena scratch;
	
	Entry *root_entry;
	
	u64 file_record_hole_count;
	u64 corrupt_usn_count;
	u64 compressed_count;
	u64 attr_list_count;
	u64 unnamed_count;
	u64 dataless_count;
	
	f64 waiting_for_io_time;
	u64 bytes_read;
	
	u64 record_size;
	
} Volume_Database;

typedef struct Work {
	Entry *entry;
	u8* data;
	u64 size;
	u64 allocated_size;
	OVERLAPPED ovl;
	File_Handle f; // async (OVERLAPEPD)
	u64 *pointer_to_match_count_for_entry;
	bool is_async;
} Work;

typedef struct Match {
	Entry *entry;
	string line;
	u64 pos_in_line;
} Match;

typedef struct Query_Context {
	u8 *read_buffer; // Circular
	u64 next_buffer_index;
	u64 buffered;
	
	Thread stream_thread;
	
	Work *work; // Persistent array, must only grow (otherwise we have race conditions)
	u64 next_work_index;
	u64 work_count;
	u64 worked_count;
	Semaphore work_sem;
	
	struct Query *query;
	Semaphore found_entry_sem;
	u64 read_entry_count;
	
	string *search_terms;
	u64 search_term_count;
	
	Arena match_strings_arena;
	Mutex match_mutex;
	
	string volume_name;
	
	bool *done_flag;
	
} Query_Context;

typedef struct Query {
	Volume_Database vol;
	Entry **filtered_entries; // persistent array
	Match *matches; // persistent array
	u64 unique_files_with_any_match_count;
	f64 start_time;
	f64 end_time;
	
	Query_Context ctx;
} Query;

typedef struct Work_Context {
	Query_Context *ctx;
} Work_Context;

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

#define MAX_CONTEXT_PREVIEW_LINE_LENGTH (80)

#define BYTES_PER_READBACK (MiB(32))
#define MAX_BYTES_PER_WORK (MiB(1)) // MUST BE MULTIPLE OF 512
unit_local u64 max_streaming_memory = GiB(3);
unit_local bool is_entire_volume_search = false;
unit_local bool is_command_line = false;

// @speed very slow but works for now
// - Sorting with a radix/base of 10. Should do 256 (that way each "digit" is a byte),
//		and the number of passes is lowered to 8 while keeping stack memory usage reasonable.
// - Maybe theres a more efficient way to do the rearranging for each pass counting sort
void radix_sort(void *items, u64 item_size, u64 item_count, u64 offset_to_sorting_number, u64 sorting_number_bit_width) {
	if (sorting_number_bit_width > 64) return;
	
	static const u64 POW10[20] = {
		1ULL,
		10ULL,
		100ULL,
		1000ULL,
		10000ULL,
		100000ULL,
		1000000ULL,
		10000000ULL,
		100000000ULL,
		1000000000ULL,
		10000000000ULL,
		100000000000ULL,
		1000000000000ULL,
		10000000000000ULL,
		100000000000000ULL,
		1000000000000000ULL,
		10000000000000000ULL,
		100000000000000000ULL,
		1000000000000000000ULL,
		10000000000000000000ULL
	};
	
	u64 nbytes = (sorting_number_bit_width+7)/8;
	
	// @todo temporary storage save point
	void *swap_space = PushTempBuffer(u8, item_size);
	
	// @speed maybe we could have an optional hint for the max value, so we can know
	// max digit count.
	u64 max_num = 0;
	for (u64 i = 0; i < item_count; i += 1) {
		u64 num = 0;
		memcpy(&num, (u8*)items+i*item_size + offset_to_sorting_number, nbytes);
		max_num = max(num, max_num);
	}
	
	u64 pass_count = 0;
	pass_count += max_num >= 1ULL;
	pass_count += max_num >= 10ULL;
	pass_count += max_num >= 100ULL;
	pass_count += max_num >= 1000ULL;
	pass_count += max_num >= 10000ULL;
	pass_count += max_num >= 100000ULL;
	pass_count += max_num >= 1000000ULL;
	pass_count += max_num >= 10000000ULL;
	pass_count += max_num >= 100000000ULL;
	pass_count += max_num >= 1000000000ULL;
	pass_count += max_num >= 10000000000ULL;
	pass_count += max_num >= 100000000000ULL;
	pass_count += max_num >= 1000000000000ULL;
	pass_count += max_num >= 10000000000000ULL;
	pass_count += max_num >= 100000000000000ULL;
	pass_count += max_num >= 1000000000000000ULL;
	pass_count += max_num >= 10000000000000000ULL;
	pass_count += max_num >= 100000000000000000ULL;
	pass_count += max_num >= 1000000000000000000ULL;
	pass_count += max_num >= 10000000000000000000ULL;
	
	for (u64 pass = 0; pass < pass_count; pass += 1) {
		u64 starts[10] = {0};
		u64 ends[10] = {0};
		
		///
		// Counting sort for each pass
		
		// Get count of each number
		for (u64 i = 0; i < item_count; i += 1) {
			u64 num = 0;
			memcpy(&num, (u8*)items+i*item_size + offset_to_sorting_number, nbytes);
			
			u64 digit = (num / POW10[pass]) % 10;
			
			starts[digit] += 1;
		}
		
		// Do the sum thing
		for (u64 i = 1; i < 10; i += 1) {
			starts[i] += starts[i-1];
		}
		
		// Do the shift thing
		for (u64 i = 9; i >= 1; i -= 1) {
			starts[i] = starts[i-1];
		}
		starts[0] = 0;
		
		for (u64 i = 0; i < 9; i += 1) {
			ends[i] = starts[i+1];
		}
		ends[9] = item_count;
		
		u64 next_free[10];
		memcpy(next_free, starts, sizeof(starts));
		
		// Sort elements in-place
		for (u64 i = 0; i < item_count; i += 1) {
			u8 *src_guy = (u8*)items+i*item_size;
			
			u64 num = 0;
			memcpy(&num, src_guy + offset_to_sorting_number, nbytes);
			u64 digit = (num / POW10[pass]) % 10;
			
			for (u64 b = 0; b < 10; b += 1) {
				if (digit == b) {
					
					if (i >= starts[b] && i < ends[b]) {
						// This guy is already in his stable sorted place.
						next_free[b] += 1;
						if (next_free[b] >= ends[b]) break;
					} else {
						u8 *dst_guy = (u8*)items+next_free[b]*item_size;
						next_free[b] += 1;
						
						memcpy(swap_space, dst_guy, item_size);
						memcpy(dst_guy, src_guy, item_size);
						memcpy(src_guy, swap_space, item_size);
						
						if (next_free[b] >= ends[b]) break;
						
						i -= 1; // We swapped a new guy into this slot whom we need to reconsider
					}
					break;
				}
			}
		}
	}
}

typedef struct Stream_Mft_Context {
	Volume_Database *vol;
	Semaphore chunk_streamed_sem;
	File_Handle f;
	Record_Range* record_ranges;
	u64 record_range_count;
	u64 file_records_per_read;
	u64 max_file_record_count;
	// Ring buffer
	u8 *buffer;
	u64 buffered;
	u64 buffer_next_read;
	u64 *chunk_sizes;
	
} Stream_Mft_Context;
s64 stream_mft_thread_proc(Thread *t) {
	tm_scope("Stream MFT Thread") {
		Stream_Mft_Context *ctx = (Stream_Mft_Context*)t->userdata;
		
		File_Handle f = ctx->f;
		Volume_Database *vol = ctx->vol;
		
		u64 buffer_next_place = 0;
		
		u64 buffer_size = ctx->max_file_record_count*vol->record_size;
		
		{
			u64 ps = sys_get_info().page_size;
			u64 page_count = (buffer_size + ps - 1) / ps;
			ctx->buffer = (u8*)sys_map_pages(SYS_MEMORY_RESERVE | SYS_MEMORY_ALLOCATE, 0, page_count, false);
		}
		
		// @speed @memory reserve
		persistent_array_init((void**)&ctx->chunk_sizes, sizeof(u64));
		
		for (u64 i = 0; i < ctx->record_range_count; i += 1) {
			Record_Range range = ctx->record_ranges[i];
			
			LARGE_INTEGER new_pointer, mv;
			mv.QuadPart = (LONGLONG)(range.first_record_index*vol->record_size);
			SetFilePointerEx(f, mv, &new_pointer, 0/*FILE_BEGIN*/);
			
			for (u64 j = 0; j < range.record_count; j += ctx->file_records_per_read) tm_scope("Read Some") {
				
				u64 file_records_to_read = min(ctx->file_records_per_read, range.record_count-j);
				u64 bytes_to_read = file_records_to_read*vol->record_size;
				
				// @todo @bug
				while (ctx->buffered + bytes_to_read >= buffer_size) {
					Sleep(1);
				}
				
				u8 *dst = ctx->buffer + (buffer_next_place % buffer_size);
				DWORD read = 0;
				BOOL ok = 0;
				tm_scope("ReadFile")
					ok = ReadFile(f, dst, (DWORD)bytes_to_read, &read, 0);
				
				DWORD err = GetLastError();
				assertmsgs(ok, tprint("%u", err));
				assert(read == bytes_to_read);
				
				buffer_next_place += bytes_to_read;
				sys_atomic_add_64(&ctx->buffered, bytes_to_read);
				
				persistent_array_push_copy(ctx->chunk_sizes, &bytes_to_read);
				
				sys_semaphore_signal(&ctx->chunk_streamed_sem);
			}
		}
	}
	return 0;
}

typedef struct Attribute_Processing_Context {
	Stream_Mft_Context *stream_ctx;
	u64 file_record_index;
	struct Scattered_Attribute *scattered_attributes; // Persistent array
	u64 sequence_number;
	bool has_name;
} Attribute_Processing_Context;

typedef struct Scattered_Attribute {
	Entry *entry;
	u64 file_record_index;
	Attribute_Processing_Context *ctx;
	Attribute_Type type;
	u16 attribute_id;
} Scattered_Attribute;

void finalize_entry(Volume_Database *vol, Attribute_Processing_Context *ctx, Entry *next_entry) {
	if (!next_entry->valid) return;
	
	//if (!(rec->flags & FILE_FLAGS_Compressed) && !(rec->flags & FILE_FLAGS_Encrypted)) {
		if (!next_entry->is_directory && !string_starts_with(next_entry->name, STR("$")) && ctx->has_name) {
			if (!next_entry->data) {
				vol->dataless_count += 1;
				//next_entry->valid = false;
			}
		}
		
		next_entry->ready = true;
	//}
	
	if (!next_entry->valid) return;
	
	//if ((rec->flags & FILE_FLAGS_Compressed) || (rec->flags & FILE_FLAGS_Encrypted)) {
	//	vol->compressed_count += 1;
	//	next_entry->valid = false; // @todo
	//}
	if (!ctx->has_name) {
		vol->unnamed_count += 1;
		next_entry->valid = false; // @todo
	} else assert(next_entry->name.count > 0 && next_entry->name.data != 0);
}

bool apply_fixups(File_Record *rec, Volume_Database *vol) {
	u16 *usa = (u16 *)((u8 *)rec + rec->offset_to_update_sequence);
	u16  usn = usa[0];
	u16  usa_count = rec->size_in_words_of_update_sequence;
	
	if (usa_count != vol->record_size / vol->boot.bytes_per_sector + 1) {
		vol->corrupt_usn_count += 1;
		return false;
	}
	
	bool err = false;
	for (u16 k = 1; k < usa_count; ++k) {
		u8 *sector_end = (u8 *)rec + k * vol->boot.bytes_per_sector - 2;
		
		if (*(u16 *)sector_end != usn) {
			vol->corrupt_usn_count += 1;
			err = true;
			break;
		}
		
		*(u16 *)sector_end = usa[k];
	}
	if (err) return false;
	
	return true;
}

bool process_attribute(Attribute_Processing_Context *ctx, Volume_Database *vol, Attribute_Header_Base *attr_header_base, Entry *next_entry) {
	
	bool attr_named = attr_header_base->name_length > 0;
	bool resident = attr_header_base->non_resident == 0;
	
	if (attr_header_base->type == ATTR_FILE_NAME /*&& !has_name  @todo, symlinks? */) {
		assert(!attr_named && resident);
		
		Attribute_Header_Resident *attr_header = (Attribute_Header_Resident *)attr_header_base;
		
		File_Name *attr = (File_Name *)(attr_header->attr_and_maybe_name);
		
		u64 name_length = attr->file_name_length*3+1;
		
		string name = string_allocate(arena_allocator(&vol->entry_names_arena), name_length);
		name.count = (u64)WideCharToMultiByte(CP_UTF8, 0, (LPCWCH)attr->name, (int)attr->file_name_length, (char*)name.data, (int)name.count, 0, 0);
		
		if (string_starts_with(name, STR("$"))) {
			next_entry->valid = false;
			return false;
		}
		
		if ((string_find_index_from_left(name, STR("~1")) == -1 || !ctx->has_name) && name.count > 0) {
			ctx->has_name = true;
			
			//u64 wasted = name_length - name.count;
			//arena_pop(&vol->entry_names_arena, wasted);
			
			if (attr->parent_file_reference != 0) {
				u64 parent_index = attr->parent_file_reference & 0x0000ffffffffffffULL;
				assert(parent_index < vol->entry_count);
				next_entry->parent = vol->entries + parent_index;
				
			} else {
				next_entry->valid = false;
			}
			
			next_entry->name = name;
			assert(next_entry->name.count > 0 && next_entry->name.data != 0);
			
			next_entry->size = attr->real_size;
		} else {
			//arena_pop(&vol->entry_names_arena, name_length);
		}
		
		if (strings_match(name, STR("."))) {
			vol->root_entry = next_entry;
		}
		
		
	} else if (attr_header_base->type == ATTR_DATA && !next_entry->is_directory && !attr_named && next_entry->valid) {
		next_entry->resident = resident;
		if (resident) {
			Attribute_Header_Resident *attr_header = (Attribute_Header_Resident *)attr_header_base;
			if (attr_header->attribute_length > 0 && attr_header->attribute_length != next_entry->size) {
				if (!next_entry->size) next_entry->size = attr_header->attribute_length;
				void *attrib_data = (u8*)attr_header + attr_header->attribute_offset;
				next_entry->data = arena_push_copy(&vol->entry_small_data_arena, attrib_data, next_entry->size);
			}
		} else {
			Attribute_Header_Non_Resident *attr_header = (Attribute_Header_Non_Resident *)attr_header_base;
			if (attr_header->attribute_real_size > 0) {
				u64 data_run_size = ((u64)attr_header->base.length_including_header - (u64)attr_header->offset_to_data_runs);
				if (!next_entry->data) {
					if (!next_entry->size) next_entry->size = attr_header->attribute_real_size;
					void *attrib_data = (u8*)attr_header + attr_header->offset_to_data_runs;
					next_entry->data = arena_push_copy(&vol->entry_data_runs_arena, attrib_data, data_run_size);
				}
			}
		}
	}
	
	
	if (attr_header_base->type == ATTR_REPARSE_POINT) tm_scope("Reparse Point") {
		if (resident) {
			
			assert(!attr_named);
			
			Attribute_Header_Resident *attr_header = (Attribute_Header_Resident *)attr_header_base;
			
			Reparse_Point *attr = (Reparse_Point *)attr_header->attr_and_maybe_name;
			
			if ((attr->flags & REPARSE_Is_Microsoft) == REPARSE_Is_Microsoft) {
				
				REPARSE_DATA_BUFFER *data = (REPARSE_DATA_BUFFER *)attr->data_and_maybe_third_party_guid;
				
				u16 *path_base = 0;
				
				if (attr->flags == (u32)IO_REPARSE_TAG_MOUNT_POINT) {
					path_base = data->DUMMYUNIONNAME.MountPointReparseBuffer.PathBuffer;
				} else if (attr->flags == (u32)IO_REPARSE_TAG_SYMLINK) {
					path_base = data->DUMMYUNIONNAME.SymbolicLinkReparseBuffer.PathBuffer;
				}
				
				if (path_base) {
					//u64 substitute_name_offset = data->DUMMYUNIONNAME.MountPointReparseBuffer.SubstituteNameOffset;
					//u64 substitute_byte_length = data->DUMMYUNIONNAME.MountPointReparseBuffer.SubstituteNameLength;
					//u64 print_name_offset      = data->DUMMYUNIONNAME.MountPointReparseBuffer.PrintNameOffset;
					//u64 print_byte_length = data->DUMMYUNIONNAME.MountPointReparseBuffer.PrintNameLength;
					
					u16 *wide_substitute_path = path_base;
					
					File_Handle subf = 0;
					tm_scope("CreateFileW")
					subf = CreateFileW(
						wide_substitute_path,
						GENERIC_READ,
						FILE_SHARE_READ | FILE_SHARE_DELETE,
						0, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0
					);
					
					if (subf == (void*)0xFFFFFFFFFFFFFFFF) {
						next_entry->valid = false;
						return false;
					}
					
					BY_HANDLE_FILE_INFORMATION info = (BY_HANDLE_FILE_INFORMATION){0};
					BOOL ok = GetFileInformationByHandle(subf, &info);
					assert(ok);
					
					u64 frn = ((u64)info.nFileIndexHigh << 32ULL) | info.nFileIndexLow;
					
					CloseHandle(subf);
					
					u64 file_record_index = frn & 0x0000FFFFFFFFFFFFULL;
					
					Entry *link_to = vol->entries + file_record_index;
					
					next_entry->is_symlink = true;
					next_entry->first_child = link_to;
				}
				
			}
			
				//if (strings_match(STR("SDF"), next_entry->name))
				//	__debugbreak();
			
		} else print("Unhandled: Non-resident reparse point.\n");
	}
	
	// This attribute is not like other attributes so it will break stuff
	if (attr_header_base->type == ATTR_LOGGED_UTILITY_STREAM) {
		next_entry->ready = true; // @todo
		return false; // break attribute looping
	}
	
	if (attr_header_base->type == ATTR_ATTRIBUTE_LIST) tm_scope("Handle ATTRIBUTE_LIST") {
		if (attr_named) print("Unhandled: named attribute list\n");
		else {
			if (resident) {
				Attribute_Header_Resident *attr_header
					= (Attribute_Header_Resident *)attr_header_base;
				Attribute_List_Record *item
					= (Attribute_List_Record*)attr_header->attr_and_maybe_name;
				
				while ((u8*)item < (u8*)attr_header+attr_header_base->length_including_header) {
					u64 ext_frn = item->base_file_reference_of_attribute;
					
					u64 ext_file_record_index = ext_frn & 0x0000ffffffffffffULL;
					u64 expected_sequence_number = ext_frn >> 48ULL;
					
					u64 stream_buffer_size = ctx->stream_ctx->max_file_record_count * vol->record_size;
					
					u64 alive_end = ctx->stream_ctx->buffer_next_read;
					u64 alive_start
						= ctx->stream_ctx->buffer_next_read >= stream_buffer_size
						? ctx->stream_ctx->buffer_next_read - stream_buffer_size
						: 0;
					assert(alive_end > alive_start);
					
					u64 byte_index = ext_file_record_index*vol->record_size;
					
					if (byte_index >= alive_start && byte_index < alive_end) {
						// This is in memory, we can handle it now
						
						File_Record *ext_rec
							= (File_Record*)(ctx->stream_ctx->buffer + (byte_index%stream_buffer_size));
						Attribute_Header_Base *ext_attr_header
							= (Attribute_Header_Base *)((u8*)ext_rec + ext_rec->first_attribute_offset);
							
						assert(strings_match((string){4, ext_rec->magic}, STR("FILE"))); // @todo err
						
						if (ext_file_record_index != ctx->file_record_index) {
						
							Entry *ext_entry = vol->entries + ext_file_record_index;
							
							if (!ext_entry->usa_applied) {
								if (!apply_fixups(ext_rec, vol)) {
									break;
								}
								
								ext_entry->usa_applied = true;
							}
							
							assert(ext_rec->mft_record_number == ext_file_record_index);
							
							
							if (ext_rec->sequence_number == expected_sequence_number) {
								assert(!ext_entry->valid);
								while ((u64)ext_attr_header < (u64)ext_rec + ext_rec->file_record_real_size && *(u32*)ext_attr_header != 0xFFFFFFFF) {
									
									if (ext_attr_header != attr_header_base && ext_attr_header->type == item->type && ext_attr_header->attribute_id == item->attribute_id) {
										if (!process_attribute(ctx, vol, ext_attr_header, next_entry)) {
											break;
										}
										ext_attr_header->type = ATTR_HANDLED;
									}
									
									
									ext_attr_header = (Attribute_Header_Base*)((u8*)ext_attr_header + ext_attr_header->length_including_header);
								}
							} else {
								static int n = 0;
								print("Outdated attribute list file record (%u)\n", n);
								n += 1;
							}
							
						}
						
						
					} else {
						if (byte_index < alive_start) {
							static u64 n = 0;
							print("Unhandled: Attribute list record not in memory (Evicted) (%u, %u)\n", n, ext_file_record_index);
							n += 1;
						} else {
							// Attribute list not in memory, postpone it until it is in memory.
							
							// We only care about these for now
							if (item->type == ATTR_FILE_NAME || item->type == ATTR_DATA || item->type == ATTR_REPARSE_POINT) {
								Scattered_Attribute *s 
									= (Scattered_Attribute *)persistent_array_push_empty(ctx->scattered_attributes);
								*s = (Scattered_Attribute){0};
								s->entry = next_entry;
								s->file_record_index = ext_file_record_index;
								s->ctx = ctx;
								s->type = item->type;
								s->attribute_id = item->attribute_id;
							}
						}
					}
					
					item = (Attribute_List_Record*)((u8*)item + item->record_length);
				}
				
			} else {
				// @todo
				static u64 n = 0;
				print("Unhandled: non-resident attribute list (%u)\n", n);
				n += 1;
				return false;
			}
		}
	}
	
	return true;
}

bool index_volume(string volume_name, Volume_Database *vol) {
	
	if (max_streaming_memory < MAX_BYTES_PER_WORK) {
		u64 n = MAX_BYTES_PER_WORK;
		print("Max streaming memory must be > %u, but it is %u\n", n, max_streaming_memory);
		return false;
	}
	
	assert(MAX_BYTES_PER_WORK % 512 == 0);
	
	*vol = (Volume_Database){0};
	
	vol->name = volume_name;
	
	string vol_file_name = tprint("\\\\.\\%s:", volume_name);
	
	u16 _cpath[MAX_PATH_LENGTH*2];
	u16 *cpath = _cpath;
	_win_utf8_to_wide(vol_file_name, cpath, MAX_PATH_LENGTH*2);
	DWORD flags = 0;
#ifdef NO_CACHE
	flags |= FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_NO_BUFFERING;
#endif
	File_Handle f = CreateFileW(
		cpath, GENERIC_READ,
		FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
		0, OPEN_EXISTING, flags, 0
	);
	if (f == (File_Handle)0xffffffffffffffff) {
		print("Could not open volume for reading.\n");
		return false;
	}
	
	// @leak
	vol->entry_names_arena = make_arena(GiB(16), MiB(10));
	vol->entry_small_data_arena = make_arena(GiB(16), MiB(10));
	vol->entry_data_runs_arena = make_arena(GiB(16), MiB(10));
	
	vol->scratch = make_arena(GiB(16), MiB(300));
	
	BYTE sector[512];
	DWORD read = 0;
	tm_scope("ReadFile boot sector")
	ReadFile(f, sector, 512, &read, 0);
	
	Ntfs_Boot_Sector *boot = (Ntfs_Boot_Sector*)sector;
	vol->boot = *boot;
	
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
	vol->record_size = record_size;
	
	LARGE_INTEGER mv;
	mv.QuadPart = (LONGLONG)mft_offset;
	LARGE_INTEGER new_pointer;
	tm_scope("SetFilePointerEx")
	SetFilePointerEx(f, mv, &new_pointer, 0/*FILE_BEGIN*/);
	
	u8* file_record_buffer = 0;
	file_record_buffer = PushTempBuffer(u8, record_size+boot->bytes_per_sector);
	file_record_buffer = (u8*)(uintptr)align_next((u64)file_record_buffer, boot->bytes_per_sector);
	
	// Read first file record
	tm_scope("Read first file record")
		ReadFile(f, file_record_buffer, (DWORD)record_size, &read, 0);
	File_Record *first_rec = (File_Record*)file_record_buffer;
	strings_match((string){4, first_rec->magic}, STR("FILE"));
	
	Record_Range record_ranges[1024] = {0};
	u64 record_range_count = 0;
	u64 total_record_count = 0;
	
	tm_scope("Find MFT Record Ranges") {
		///
		// Assume first file record is $MFT and find all file record ranges
		
		File_Name *file_name_attrib = 0;
		Attribute_Header_Non_Resident *data_attrib = 0;
		
		Attribute_Header_Base *attr_base = (Attribute_Header_Base *)((u8*)first_rec + first_rec->first_attribute_offset);
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
	
	u64 ps = sys_get_info().page_size;
	
	vol->entry_count = total_record_count;
	u64 entries_size = vol->entry_count*sizeof(Entry);
	u64 entries_mem_size = align_next(entries_size, 8);
	{
		u64 page_count  = (entries_mem_size + ps + 1) / ps;
		// @leak
		u8 *entries_mem = (u8*)sys_map_pages(SYS_MEMORY_RESERVE | SYS_MEMORY_ALLOCATE, 0, page_count, false);
		vol->entries = (Entry*)entries_mem;
		memset(vol->entries, 0, entries_mem_size);
	}
	
	////////////
	/// Walk file records. Read future file records from disk asynchronously.
	
	
	Scattered_Attribute *scattered_attributes;
	persistent_array_init((void**)&scattered_attributes, sizeof(Scattered_Attribute));
	
	Attribute_Processing_Context *attribute_contexts;
	persistent_array_init((void**)&attribute_contexts, sizeof(Attribute_Processing_Context));
	
	Stream_Mft_Context stream_ctx = (Stream_Mft_Context){0};
	
	stream_ctx.vol = vol;
	stream_ctx.f = f;
	sys_semaphore_init(&stream_ctx.chunk_streamed_sem);
	stream_ctx.record_ranges = record_ranges;
	stream_ctx.record_range_count = record_range_count;
	stream_ctx.file_records_per_read = BYTES_PER_READBACK/record_size;
	u64 chunk_count = max_streaming_memory/(stream_ctx.file_records_per_read*record_size);
	stream_ctx.max_file_record_count = chunk_count * stream_ctx.file_records_per_read;
	
	Thread stream_thread;
	sys_thread_init(&stream_thread, stream_mft_thread_proc, &stream_ctx);
	sys_thread_start(&stream_thread);
	
	u64 stream_buffer_size = stream_ctx.max_file_record_count * record_size;
	
	u64 total_processed_count = 0;
	
	u64 chunk_index = 0;
	
	tm_scope("Process streamed chunks")
	while (total_processed_count < vol->entry_count) tm_scope("Process one chunk") {
		
		tm_scope("Wait chunk streamed")
		sys_semaphore_wait(stream_ctx.chunk_streamed_sem);
		
		u8 *chunk = stream_ctx.buffer + (stream_ctx.buffer_next_read % stream_buffer_size);
		
		u64 chunk_size = stream_ctx.chunk_sizes[chunk_index];
		assert(chunk_size % record_size == 0);
		u64 streamed_count = chunk_size/record_size;
		
		stream_ctx.buffer_next_read += chunk_size;
		
		
		/////
		// On receiving new chunk, check if any scattered attributes happened to be loaded in
		tm_scope("Check scattered attributes") {
			u64 file_index_a = total_processed_count;
			u64 file_index_b = file_index_a + streamed_count;
			
			for (u64 i = 0; i < persistent_array_count(scattered_attributes); i += 1) {
				Scattered_Attribute *s = scattered_attributes + i;
				Entry *next_entry = s->entry;
				
				u64 count = persistent_array_count(scattered_attributes);
				
				if (s->file_record_index >= file_index_a && s->file_record_index < file_index_b) {
					File_Record *ext_rec = (File_Record*)(chunk + (s->file_record_index - file_index_a)*record_size);
					
					Entry *ext_entry = vol->entries + s->file_record_index;
					
					if (!ext_entry->usa_applied) {
						if (!apply_fixups(ext_rec, vol)) {
							// Unordered remove
							if (i < count-1) {
								*s = scattered_attributes[count-1];
								i -= 1;
							}
							persistent_array_set_count(scattered_attributes, (u64)(count-1));
							
							continue;
						}
						ext_entry->usa_applied = true;
					}
					
					assert(strings_match((string){4, ext_rec->magic}, STR("FILE"))); // @todo err
					
					next_entry->valid = true;
					
					Attribute_Header_Base *ext_attr_header
						= (Attribute_Header_Base *)((u8*)ext_rec + ext_rec->first_attribute_offset);
					
					assert(ext_rec->mft_record_number == s->file_record_index);
					
					while ((u64)ext_attr_header < (u64)ext_rec + ext_rec->file_record_real_size && *(u32*)ext_attr_header != 0xFFFFFFFF) {
						
						if (ext_attr_header->type == s->type && ext_attr_header->attribute_id == s->attribute_id) {
							if (!process_attribute(s->ctx, vol, ext_attr_header, next_entry)) {
								break;
							}
							ext_attr_header->type = ATTR_HANDLED;
						}
						
						ext_attr_header = (Attribute_Header_Base*)((u8*)ext_attr_header + ext_attr_header->length_including_header);
					}
					
					finalize_entry(vol, s->ctx, next_entry);
					
					// Unordered remove
					// @todo try to see if ordered remove makes things overall faster or slower
					// (Scattered attributes might appear in order, so it might be overall faster
					// to find the attributes we need if they are ordered).
					if (i < count-1) {
						*s = scattered_attributes[count-1];
						i -= 1;
					}
					persistent_array_set_count(scattered_attributes, (u64)(count-1));
					
				}
				
			}
		}
		
		tm_scope("Process records in chunk")
		for (u64 i = 0; i < streamed_count; i += 1) {
			File_Record *rec = (File_Record*)(chunk + record_size*i);
			
			
			Attribute_Processing_Context *ctx = 
				(Attribute_Processing_Context *)persistent_array_push_empty(attribute_contexts);
			*ctx = (Attribute_Processing_Context){0};
			
			ctx->stream_ctx = &stream_ctx;
			ctx->file_record_index = total_processed_count + i;
			ctx->scattered_attributes = scattered_attributes;
			ctx->sequence_number = rec->sequence_number;
			
			Entry *next_entry = vol->entries + (total_processed_count + i);
			// Note: DO NOT ZERO HERE
			assert(!next_entry->valid);
			assert(next_entry->name.count == 0);
			
			// @copypaste
			if (!next_entry->usa_applied) {
				if (!apply_fixups(rec, vol)) {
					continue;
				}
				
				next_entry->usa_applied = true;
			}
			
			// This is a dummy file record to hold ATTRIBUTE_LIST entries
			// NOTE: This check must happen after usa fixups
			if (rec->base_file_record_reference != 0) {
				continue;
			}
			
			if (!strings_match((string){4, rec->magic}, STR("FILE"))) {
				vol->file_record_hole_count += 1;
				continue;
			}
			
			next_entry->valid = true;
			
			next_entry->is_directory = (rec->flags & FILE_RECORD_FLAGS_DIRECTORY) != 0;
			Attribute_Header_Base *attr_header_base = (Attribute_Header_Base *)((u8*)rec + rec->first_attribute_offset);
			
			/////
			// Walk attributes
			
			//tm_scope("Walk attributes")
			while ((u64)attr_header_base < (u64)rec + rec->file_record_real_size && *(u32*)attr_header_base != 0xFFFFFFFF) {
				
				if (!process_attribute(ctx, vol, attr_header_base, next_entry)) {
					break;
				}
					
				attr_header_base->type = ATTR_HANDLED;
				attr_header_base = (Attribute_Header_Base*)((u8*)attr_header_base + attr_header_base->length_including_header);
			}
			
			
			finalize_entry(vol, ctx, next_entry);
		}
		
		total_processed_count += streamed_count;
		
		sys_atomic_add_64(&stream_ctx.buffered, -chunk_size);
		
		chunk_index += 1;
	}
	
	sys_thread_join(&stream_thread);
	sys_thread_close(&stream_thread);
	sys_semaphore_release(&stream_ctx.chunk_streamed_sem);
	
	sys_unmap_pages(stream_ctx.buffer);
	persistent_array_uninit(stream_ctx.chunk_sizes);
	
	persistent_array_uninit(scattered_attributes);
	persistent_array_uninit(attribute_contexts);
	
	for (u64 i = 0; i < vol->entry_count; i += 1) {
		Entry *entry = vol->entries + i;
		
		if (!entry->valid) continue;
		
		Entry *p = entry->parent;
		
		if (entry == p) continue; // ".", or other metafiles, etc
		
		if (!p->first_child) {
			p->first_child = entry;
		} else {
			entry->next_sibling = p->first_child;
			p->first_child = entry;
		}
	}
	
	for (u64 i = 0; i < vol->entry_count; i += 1) {
		Entry *entry = vol->entries + i;
		
		if (!entry->valid) continue;
		
		if (entry->is_symlink) {
			string pretend_name = entry->name;
			Entry *pretend_parent = entry->parent;
			Entry *pretend_sibling = entry->next_sibling;
			
			Entry *link_to = entry->first_child;
			
			memcpy(entry, link_to, sizeof(Entry));
			
			entry->name = pretend_name;
			entry->parent = pretend_parent;
			entry->next_sibling = pretend_sibling;
		}
		
		Entry *p = entry->parent;
		
		if (entry == p) continue; // ".", or other metafiles, etc
		
		// Sometimes there are weird duplicates of directory names.
		// For now we work around this by ignoring empty directores
		// (the duplicates seem to be empty)
		// @todo @bug
		if (entry->is_directory && !entry->first_child) {
			entry->valid = false;
		}
	}
	
	free_arena(vol->scratch);
	
	CloseHandle(f);
	
	vol->done = true;
	return true;
}

/*void update_volume_blocking(Volume_Database *state, bool *keep_going_flag) {
	while (*keep_going_flag) {
		
	}
}*/

string tprint_entry_full_path(string volume_label, Entry *entry) {
	string dirpath = STR("");
	
	Entry *parent = entry->parent;
	while (parent && parent != parent->parent && !strings_match(parent->name, STR("."))) {
		dirpath = tprint("%s\\%s", parent->name, dirpath);
		
		parent = parent->parent;
	}
	
	dirpath = tprint("%s:\\%s", volume_label, dirpath);
	
	return tprint("%s%s", dirpath, entry->name);
}

typedef enum Task_Kind {
	TASK_INDEX_VOLUME,
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
	string *search_terms;
	u64 search_term_count;
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

Entry *full_path_to_entry_directory(Volume_Database vol, string absolute_path) {
	timestamp_begin(STR("Find directory entry"));
	
	if (absolute_path.count == 2 || absolute_path.count == 3) {
		if (string_starts_with(vol.name, absolute_path) || string_starts_with(absolute_path, vol.name)) {
			return vol.root_entry;
		} else {
			return 0;
		}
	}
	
	u64 slashes = string_count_occurences(absolute_path, STR("\\"));
	Entry *path_entry = 0;
	
	if (slashes > 0) {
		u64 search_dir_name_count = 0;
		string *search_dir_names = PushTempBuffer(string, slashes+1);
		
		string next = absolute_path;
		for (u64 i = 0; i < slashes; i += 1) {
			s64 slash_index = string_find_index_from_right(next, STR("\\"));
			search_dir_names[search_dir_name_count++] = string_slice(next, (u64)slash_index+1, next.count-(u64)slash_index-1);
			next = string_slice(next, 0, (u64)slash_index);
		}
		search_dir_names[search_dir_name_count++] = STR(".");
		
		for (u64 i = 0; i < vol.entry_count; i += 1) {
			
			u64 dir_match_count = 0;
			
			Entry *entry = &vol.entries[i];
			
			if (!entry->valid) continue;
			if (!entry->is_directory) continue;
			
			Entry *next_dir = entry;
			for (u64 j = 0; j < search_dir_name_count; j += 1) {
				
				if (!next_dir->valid) break;
				
				if (strings_match(next_dir->name, search_dir_names[j])) {
					dir_match_count += 1;
				} else {
					dir_match_count = 0;
				}
				
				if (strings_match(next_dir->name, STR("."))) {
					break;
				}
				
				next_dir = next_dir->parent;
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

s64 work_thread_proc(Thread *t) {
	tm_scope("Worker thread") {
		Work_Context *wctx = (Work_Context*)t->userdata;
		Query_Context *ctx = wctx->ctx;
		while (1) {
			tm_scope("Wait for work")
			sys_semaphore_wait(ctx->work_sem);
			
			u64 work_index = sys_atomic_add_64(&ctx->next_work_index, 1);
			Work *work = ctx->work + work_index;
			
			if (work_index >= persistent_array_count(ctx->work)) {
				break;
			}
			
			assert(work->data);
			assert(work->size);
			
			
			if (work->is_async) tm_scope("Wait for async ReadFile") {
				DWORD read = 0;
				BOOL ok = GetOverlappedResult(work->f, &work->ovl, &read, TRUE);
				DWORD err = GetLastError();
				assertmsgs(ok, tprint("%u", err));
				//assert(read == work->size);
				CloseHandle(work->ovl.hEvent);
			}
			
			//print("Read @ %u\n", work->data);
			
			u8 *p = work->data;
			u8 *end = p + work->size;
			
			u8 *line_start = p;
			
			// @simd
			tm_scope("Search data chunk for match")
			while (p < end) {
				
				if (p > work->data && *(p-1) == '\n') {
					line_start = p;
				}
				
				for (u64 i = 0; i < ctx->search_term_count; i += 1) {
					string term = ctx->search_terms[i];
					
					if (p + term.count <= end) {
						
						bool does_match = true;
						
						for (u64 j = 0; j < term.count; j += 1) {
							if (p[j] != term.data[j]) {
								does_match = false;
								break;
							}
						}
						
						if (does_match) tm_scope("Handle match") {
							Match match = (Match) {0};
							match.entry = work->entry;
							match.pos_in_line = (u64)(p - line_start);
							
							u8 *line_end = p;
							
							while (line_end < end && *line_end != '\n') {
								line_end += 1;
							}
							
							string line_slice = STR("");
							
							if ((u64)(line_end-line_start) <= MAX_CONTEXT_PREVIEW_LINE_LENGTH) {
								line_slice = (string) { (u64)(line_end-line_start), line_start };
							} else if (term.count < MAX_CONTEXT_PREVIEW_LINE_LENGTH) {
								u64 budget = MAX_CONTEXT_PREVIEW_LINE_LENGTH - term.count;
								u64 left_budget = budget/2;
								u64 right_budget = budget/2;
								if (budget % 2 == 0) left_budget += 1;
								
								u8 *left = max((p-left_budget), line_start);
								right_budget += (u64)left - (u64)(p-left_budget);
								u8 *right = min((p+right_budget), line_end);
								left_budget += (u64)(p+right_budget) - (u64)right;
								
								left  = max((p-left_budget), line_start);
								right = min((p+right_budget), line_end);
								
								line_slice = (string) { (u64)(right-left), left };
								
							} else {
								line_slice = string_slice(term, 0, MAX_CONTEXT_PREVIEW_LINE_LENGTH);
							}
							
							
							match.line.count = line_slice.count;
							
							// @speed this mutex might noticably affect responsiveness if there are
							// a lot of matches.
							sys_mutex_acquire(ctx->match_mutex);
							match.line.data = (u8*)arena_push_string(&ctx->match_strings_arena, line_slice);
							persistent_array_push_copy(ctx->query->matches, &match);
							u64 c = sys_atomic_add_64(work->pointer_to_match_count_for_entry, 1);
							if (c == 0) sys_atomic_add_64(&ctx->query->unique_files_with_any_match_count, 1);
							sys_mutex_release(ctx->match_mutex);
						}
					}
				}
				p += 1;
			}
			
			work->entry->was_searched = true; // @todo this will be true after 1st data run, but there will often be more data runs.
			
			work->entry = (void*)0xFEEDFACEDEADBEEF;
			sys_atomic_add_64(&ctx->buffered, (u64)-work->allocated_size);
			assert(ctx->buffered < max_streaming_memory);
			sys_atomic_add_64(&ctx->work_count, (u64)-1);
			*work = (Work){0};
		}
	}
	return 0;
}

s64 stream_file_data_thread_proc(Thread *t) {
	tm_scope("Stream file data Thread") {
		Query_Context *ctx = (Query_Context*)t->userdata;
		
		System_Info sys_info = sys_get_info();
		u64 thread_count = (u64)max(((f64)sys_info.logical_cpu_count * 0.75), 1);
		Thread *work_threads = PushTempBuffer(Thread, thread_count);
		Work_Context *work_contexts = PushTempBuffer(Work_Context, thread_count);
		
		for (u64 i = 0; i < thread_count; i += 1) {
			Thread *tw = work_threads + i;
			Work_Context *wctx = work_contexts + i;
			*wctx = (Work_Context){0};
			wctx->ctx = ctx;
			
			sys_thread_init(tw, work_thread_proc, wctx);
			sys_thread_start(tw);
		}
		
		
		string vol_file_name = tprint("\\\\.\\%s:", ctx->volume_name);
		
		u16 _cpath[MAX_PATH_LENGTH*2];
		u16 *cpath = _cpath;
		_win_utf8_to_wide(vol_file_name, cpath, MAX_PATH_LENGTH*2);
		DWORD flags = FILE_FLAG_OVERLAPPED;
#ifdef NO_CACHE
		flags |= FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_NO_BUFFERING;
#endif
		
		File_Handle f = CreateFileW(
			cpath, GENERIC_READ,
			FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
			0, OPEN_EXISTING, /*FILE_FLAG_RANDOM_ACCESS | FILE_FLAG_NO_BUFFERING |*/ flags, 0
		);
		
		if (f == (File_Handle)0xffffffffffffffff) {
			DWORD e = GetLastError();
			print("stream_file_data_thread_proc: Could not open volume for reading. %u\n", e);
			return 1;
		}
		
		u64 *unique_file_counts;
		{
			u64 ps = sys_info.page_size;
			u64 sz = ctx->query->vol.entry_count*sizeof(u64); // @memory this is way more than we need
			u64 page_count = (sz+ps-1)/ps;
			unique_file_counts = (u64*)sys_map_pages(SYS_MEMORY_RESERVE | SYS_MEMORY_ALLOCATE, 0, page_count, false);
			// @todo ?
			assertmsg(unique_file_counts, "Out of memory");
			memset(unique_file_counts, 0, sz);
		}
		
		tm_scope("Loop")
		while (1) {
			reset_temporary_storage();
			
			tm_scope("Wait for Semaphore")
			sys_semaphore_wait(ctx->found_entry_sem);
			
			if (ctx->read_entry_count >= persistent_array_count(ctx->query->filtered_entries)) {
				break;
			}
			
			u64 entry_index = ctx->read_entry_count++;
			Entry *entry = ctx->query->filtered_entries[entry_index];
			
			if (!entry->valid) {
				//entry->was_searched = true;
				continue;
			}
			
			if (!entry->data) {
				//entry->was_searched = true;
				// @todo, some file entries are missing DATA attribute, and it seems the data is to be found elsewhere.
				continue;
			}
			
			
			u8 *data = entry->data;
			
			// @cleanup data runs can theoretically be larger than max streaming memory, but we should fragment large data runs anyways.
			// @todo notify user somehow if this happens, and what entries are skipped.
			//if (entry->size > max_streaming_memory) {
			//	continue;
			//}
			
			tm_scope("Resident entry")
			if (entry->resident) {
				
				////////////
				/// Resident file content (small, fits in 1kb file record)
				
				if (!entry->size) {
					entry->was_searched = true;
					continue;
				}
				
				// Busy-wait for there to be free readback memory. This shouldn't really hit
				// unless user has a very slow CPU, or/and very low max_streaming_memory.
				// @todo @bug This can cause program to hang. @copypaste
				// Make sure this will exit if main thread wants to exit.
				while (ctx->buffered+entry->size >= max_streaming_memory) {
					Sleep(1);
				}
				
				// @copypaste
				// Next location in read_buffer
				u8 *next_read_buffer_pointer
				= ctx->read_buffer + (ctx->next_buffer_index % max_streaming_memory);
				
				Work *w0 = 0, *w1 = 0;
				
				w0 = (Work *)persistent_array_push_empty(ctx->work);
				*w0 = (Work){0};
				w0->entry = entry;
				w0->pointer_to_match_count_for_entry = unique_file_counts + entry_index;
				w0->data = next_read_buffer_pointer;
				
				// This chunk might cycle around to the start of the ring buffer.
				// So, we need to fragment it into two.
				if (next_read_buffer_pointer + entry->size > ctx->read_buffer + max_streaming_memory) {
					u64 s0 =
					(u64)(ctx->read_buffer + max_streaming_memory) - (u64)next_read_buffer_pointer;
					s0 = align_next(s0, 512);
					
					w0->size = s0;
					
					u64 s1 = entry->size - s0;
					
					w1 = (Work *)persistent_array_push_empty(ctx->work);
					*w1 = (Work){0};
					w1->entry = entry;
					w1->pointer_to_match_count_for_entry = unique_file_counts + entry_index;
					w1->data = ctx->read_buffer;
					w1->size = s1;
					
				} else {
					w0->size = entry->size;
				}
				
				w0->allocated_size = w0->size;
				if (w1) w1->allocated_size = w1->size;
				
				memcpy(w0->data, data, w0->size);
				sys_atomic_add_64(&ctx->buffered, w0->allocated_size);
				assert(ctx->buffered < max_streaming_memory);
				sys_atomic_add_64(&ctx->work_count, 1);
				sys_semaphore_signal(&ctx->work_sem);
				if (w1) {
					//assert(false); // remove
					memcpy(w1->data, data + w0->size, w1->size);
					sys_atomic_add_64(&ctx->buffered, w1->allocated_size);
					assert(ctx->buffered < max_streaming_memory);
					sys_atomic_add_64(&ctx->work_count, 1);
					sys_semaphore_signal(&ctx->work_sem);
				}
				
				
				ctx->next_buffer_index += align_next(entry->size, 512);
				
				
			} else tm_scope("Non-resident entry") {
				
				////////////
				/// Non-resident file content (large, scattered in data runs)
				///  We need to parse the data runs.
				
				u64 previous_cluster = 0;
				u8 *p = (u8*)data;
				
				u64 total_data_visited = 0;
				
				while (*p != 0) {
					
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
					
					u64 offset_in_bytes = start_cluster  * ctx->query->vol.boot.sectors_per_cluster * ctx->query->vol.boot.bytes_per_sector;
					u64 length_in_bytes = cluster_length * ctx->query->vol.boot.sectors_per_cluster * ctx->query->vol.boot.bytes_per_sector;
					assert(offset_in_bytes % ctx->query->vol.record_size == 0);
					assert(length_in_bytes % ctx->query->vol.record_size == 0);
					if (total_data_visited + length_in_bytes > entry->size) {
						length_in_bytes -= (total_data_visited + length_in_bytes) - entry->size;
					}
					
					// @todo @bug This can cause program to hang. @copypaste
					// Make sure this will exit if main thread wants to exit.
					// Timeout ?
					while (ctx->buffered+length_in_bytes >= max_streaming_memory) {
						Sleep(1);
					}
					
					assert(ctx->next_buffer_index % 512 == 0);
					
					u64 work_submitted = 0;
					while (work_submitted < length_in_bytes) {
						
						assert(max_streaming_memory % 512 == 0);
						
						u64 work_size = min(MAX_BYTES_PER_WORK, length_in_bytes-work_submitted);
						
						// Only the end of data runs should be able to be padded,
						// Because MAX_BYTES_PER_WORK should be a multiple of 512
						u64 work_size_padded = align_next(work_size, 512);
						
						u64 work_offset = offset_in_bytes + work_submitted;
						
						Work *w0 = 0, *w1 = 0;
						
						w0 = (Work *)persistent_array_push_empty(ctx->work);
						*w0 = (Work){0};
						w0->entry = entry;
						w0->pointer_to_match_count_for_entry = unique_file_counts + entry_index;
						w0->f = f;
						w0->is_async = true;
						w0->data = ctx->read_buffer + ((ctx->next_buffer_index+work_submitted) % max_streaming_memory);
						assert((u64)w0->data % 512 == 0);
						
						// This chunk might cycle around to the start of the ring buffer.
						// So, we need to fragment it into two.
						if (w0->data + work_size_padded > ctx->read_buffer + max_streaming_memory) {
							//assert(false); // remove
							u64 s0 =
							(u64)(ctx->read_buffer + max_streaming_memory) - (u64)w0->data;
							u64 s1 = work_size_padded - s0;
							
							assert(s0 % 512 == 0);
							w0->size = s0;
							w0->allocated_size = s0;
							
							w1 = (Work *)persistent_array_push_empty(ctx->work);
							*w1 = (Work){0};
							w1->entry = entry;
							w1->pointer_to_match_count_for_entry = unique_file_counts + entry_index;
							w1->f = f;
							w1->is_async = true;
							w1->data = ctx->read_buffer;
							w1->size = s1;
							w1->allocated_size = align_next(s1, 512);
						} else {
							w0->size = work_size;
							w0->allocated_size = work_size_padded;
						}
						
						w0->ovl = (OVERLAPPED){0};
						w0->ovl.Offset = work_offset & 0xFFFFFFFF;
						w0->ovl.OffsetHigh = work_offset >> 32;
						w0->ovl.hEvent = CreateEventA(0, 1, 0, 0);
						//print("Write @ %u\n", w0->data);
						BOOL ok = ReadFile(f, w0->data, (DWORD)w0->allocated_size, 0, &w0->ovl);
						sys_atomic_add_64(&ctx->buffered, w0->allocated_size);
						assert(ctx->buffered < max_streaming_memory);
						DWORD err = GetLastError();
						assertmsgs(!ok || err == 997 /*ERROR_IO_PENDING*/, tprint("%u", err));
						
						sys_atomic_add_64(&ctx->work_count, 1);
						sys_semaphore_signal(&ctx->work_sem); // signal w0
						
						if (w1) {
							//assert(false); // remove
							w1->ovl = (OVERLAPPED){0};
							w1->ovl.Offset = (work_offset + w0->size) & 0xFFFFFFFF;
							w1->ovl.OffsetHigh = (work_offset + w0->size) >> 32;
							w1->ovl.hEvent = CreateEventA(0, 1, 0, 0);
							
							ok = ReadFile(f, w1->data, (DWORD)w1->allocated_size, 0, &w1->ovl);
							sys_atomic_add_64(&ctx->buffered, w1->allocated_size);
							assert(ctx->buffered < max_streaming_memory);
							err = GetLastError();
							assertmsgs(!ok || err == 997 /*ERROR_IO_PENDING*/, tprint("%u", err));
							
							sys_atomic_add_64(&ctx->work_count, 1);
							sys_semaphore_signal(&ctx->work_sem); // signal w1
						}
						
						work_submitted += work_size;
					}
					
					ctx->next_buffer_index += align_next(length_in_bytes, 512);
					
					p += cluster_offset_size;
					total_data_visited += length_in_bytes;
				}
			}
		}
		
		tm_scope("Wait workers & cleanup") {
			for (u64 i = 0; i < thread_count; i += 1) {
				sys_semaphore_signal(&ctx->work_sem);
			}
			print("Stream thread waiting for threads...\n");
			for (u64 i = 0; i < thread_count; i += 1) {
				Thread *tw = work_threads + i;
				sys_thread_join(tw);
				sys_thread_close(tw);
			}
			print("Stream thread done\n");
			
			ctx->query->end_time = sys_get_seconds_monotonic();
			if (ctx->done_flag) *ctx->done_flag = true;
			
			CloseHandle(f);
			
			sys_unmap_pages(unique_file_counts);
		}
	}
	return 0;
}


typedef struct Child_Iterator {
	Entry *last;
	Entry *parent_stack[1024]; // Symlink reasons
	u64 parent_count;
} Child_Iterator;

Entry *get_next_child(Child_Iterator *it, Entry *directory) {
	Entry *cur = it->last;

	if (!cur) {
		it->parent_count = 0;
		it->last = (directory && directory->first_child) ? directory->first_child : 0;
		if (it->last) {
			it->parent_stack[it->parent_count++] = directory;
		}
		return it->last;
	}

	if (cur->is_directory && cur->first_child) {
		if (it->parent_count < (sizeof(it->parent_stack) / sizeof(it->parent_stack[0]))) {
			it->parent_stack[it->parent_count++] = cur;
			it->last = cur->first_child;
			return it->last;
		}
	}

	if (cur->next_sibling) {
		it->last = cur->next_sibling;
		return it->last;
	}

	while (it->parent_count > 0) {
		Entry *parent = it->parent_stack[it->parent_count - 1];
		it->parent_count--;

		if (parent == directory) {
			it->last = 0;
			return 0;
		}

		if (parent->next_sibling) {
			it->last = parent->next_sibling;
			return it->last;
		}
	}

	it->last = 0;
	return 0;
}

void query_volume(Query *query, Volume_Database vol, Entry *directory, string *filters, u64 filter_count, string *search_terms, u64 search_term_count, bool *done_flag) {
	f64 start_time = sys_get_seconds_monotonic();
	*query = (Query) {0};
	query->vol = vol;
	query->start_time = start_time;
	
	if (done_flag) *done_flag = false;
	
	query->ctx = (Query_Context){0};
	{
		u64 ps = sys_get_info().page_size;
		u64 page_count = (max_streaming_memory+ps+512)/ps;
		query->ctx.read_buffer = sys_map_pages(SYS_MEMORY_RESERVE | SYS_MEMORY_ALLOCATE, 0, page_count, false);
		query->ctx.read_buffer = (u8*)(uintptr)align_next((u64)query->ctx.read_buffer, 512);
		
		persistent_array_init((void**)&query->ctx.work, sizeof(Work));
		
		query->ctx.query = query;
		
		query->ctx.search_terms = search_terms;
		query->ctx.search_term_count = search_term_count;
		
		sys_semaphore_init(&query->ctx.found_entry_sem);
		sys_semaphore_init(&query->ctx.work_sem);
		sys_mutex_init(&query->ctx.match_mutex);
		query->ctx.match_strings_arena = make_arena(GiB(16), MiB(1));
		
		query->ctx.volume_name = vol.name;
		
		query->ctx.done_flag = done_flag;
	}
	
	persistent_array_init((void**)&query->filtered_entries, sizeof(Entry*));
	persistent_array_init((void**)&query->matches, sizeof(Match));
	
	
	if (search_term_count > 0) {
		sys_thread_init(&query->ctx.stream_thread, stream_file_data_thread_proc, &query->ctx);
		sys_thread_start(&query->ctx.stream_thread);
	}
	
	timestamp_begin(STR("Find children which match filter"));
	
	if (filter_count == 0) {
		Child_Iterator it = (Child_Iterator){0};
		Entry *entry;
		while ((entry = get_next_child(&it, directory))) {
			if (entry->is_directory) continue; // @todo Display directories when searching file names, but differently
			entry->was_searched = false;
			persistent_array_push_copy(query->filtered_entries, &entry);
			sys_semaphore_signal(&query->ctx.found_entry_sem);
		}
	} else {
		Child_Iterator it = (Child_Iterator){0};
		Entry *entry;
		
		while ((entry = get_next_child(&it, directory))) {
			
			if (!entry->valid) continue;
			
			bool any_match = false;
			
			string name = entry->name;
			
			for (u64 k = 0; k < filter_count; k += 1) {
				string filter = filters[k];
				bool prefix_wildcard = filter.data[0] == '*';
				bool suffix_wildcard = filter.data[filter.count-1] == '*';
				
				if (strings_match(string_trim(filter), STR("*"))) {
					if (entry->is_directory) continue; // @todo Display directories when searching file names, but differently
					any_match  = true;
					break;
				}
				
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
				entry->was_searched = false;
				persistent_array_push_copy(query->filtered_entries, &entry);
				sys_semaphore_signal(&query->ctx.found_entry_sem);
			}
			
		}
	}
	timestamp_end(STR("Find children which match filter"));
	
	sys_semaphore_signal(&query->ctx.found_entry_sem);
}

void query_free(Query *query) {
	
	print("Query free\n"); // @cleanup
	
	if (query->ctx.search_term_count > 0) {
		sys_thread_join(&query->ctx.stream_thread);
		sys_thread_close(&query->ctx.stream_thread);
	}
	
	sys_unmap_pages(query->ctx.read_buffer);
	persistent_array_uninit(query->ctx.work);
	sys_semaphore_release(query->ctx.found_entry_sem);
	sys_semaphore_release(query->ctx.work_sem);
	sys_mutex_uninit(&query->ctx.match_mutex);
	free_arena(query->ctx.match_strings_arena);
	
	persistent_array_uninit(query->filtered_entries);
	persistent_array_uninit(query->matches);
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

void print_stats(Volume_Database vol, Query query) {
	print("Volume Database:\n");
	print("\tfile_record_hole_count: %u\n", vol.file_record_hole_count);
	print("\tcorrupt_usn_count: %u\n", vol.corrupt_usn_count);
	print("\tcompressed_count: %u\n", vol.compressed_count);
	print("\tattr_list_count: %u\n", vol.attr_list_count);
	print("\tunnamed_count: %u\n", vol.unnamed_count);
	print("\tdataless_count: %u\n", vol.dataless_count);
	f64 ms = vol.waiting_for_io_time * 1024;
	print("\tTime idle waiting for disk: %f ms\n", ms);
	f64 mb = (f64)vol.bytes_read/1024.0/1024.0;
	print("\tFile record bytes read: %f MB\n", mb);
	
	u64 entry_names_size = ((u64)vol.entry_names_arena.position - (u64)vol.entry_names_arena.start);
	u64 small_data_size = ((u64)vol.entry_small_data_arena.position - (u64)vol.entry_small_data_arena.start);
	u64 data_runs_size = ((u64)vol.entry_data_runs_arena.position - (u64)vol.entry_data_runs_arena.start);
	u64 entries_size = vol.entry_count * sizeof(Entry);
	
	mb = (f64)(entry_names_size + entries_size + small_data_size + data_runs_size) / MiB(1);
	print("Volume database RAM usage: %f MB\n", mb);
	mb = (f64)entry_names_size / MiB(1);
	print("\tEntry Names: %f MB\n", mb);
	mb = (f64)small_data_size / MiB(1);
	print("\tSmall File Data: %f MB\n", mb);
	mb = (f64)data_runs_size / MiB(1);
	print("\tData Runs: %f MB\n", mb);
	mb = (f64)entries_size / MiB(1);
	print("\tIndexed Entry Data: %f MB\n", mb);
	
	print("Query:\n");
	print("\tTotal file records processed: %u\n", vol.entry_count);
	u64 c = persistent_array_count(query.filtered_entries);
	print("\tFiltered file records: %u\n", c);
	c = persistent_array_count(query.filtered_entries);
	print("\tFiltered file records: %u\n", c);
	for (u64 i = 0; i < timestamp_count; i += 1) {
		Timestamp *ts = &timestamps[i];
		ms = ts->accum * 1000.0;
		
		print("\t\t%s: %f ms\n", ts->label, ms);
	}
}

int main(int argc, char **argv) {
	
	max_streaming_memory = align_next(max_streaming_memory, 512);
	
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
		
		string volume_name = string_slice(search_directory, 0, (u64)colon_index);
		
		//vol_file_name = tprint("\\\\.\\%s:", volume_name);
		
		Volume_Database vol;
		
		bool vol_ok = index_volume(volume_name, &vol);
		
		if (!vol_ok) {
			print("Indexing volume failed.\n");
			sys_exit(1);
		}
		
		timestamp_end(STR("Index Volume"));
		
		Entry *search_directory_entry = full_path_to_entry_directory(vol, search_directory);
		
		if (!search_directory_entry) {
			print("Could not find search directory entry\n");
		}
		
		u64 search_term_count = 1;
		
		if (search_term.count == 0 || strings_match(search_term, STR("*"))) {
			search_term_count = 0;
		}
		
		Query query;
		query_volume(&query, vol, search_directory_entry, filters, filter_count, &search_term, search_term_count, 0);
		assert(false); // @todo wait for query to complete
		
		//for (u64 i = 0; i < persistent_array_count(query.filtered_entries); i += 1) {
		//	string path = tprint_entry_full_path(volume_name, query.filtered_entries[i]);
		//	print("%s\n", path);
		//}
		
		print_stats(vol, query);
		
		
		print("Goodbye, Terminal.\n");
	}
	
	dump_profiler_result();
	
	return 0;
}




int gui_callback(struct nk_context *ctx)
{
	static char dir[MAX_PATH]         = "C:/The-Forge/Examples_3/Unit_Tests/PC_VS2019/x64/Debug/09_LightShadowPlayground/SDF";
	//static char dir[MAX_PATH]         = "C:";
	static char filter[MAX_FILTER]    = "*.c, *.cpp, *.h, *.hpp, *.txt, *.json, *.xml, *.html";
	static char search_str[MAX_SEARCH]= "Hello";
	static string search_term;
	static Query queries[1024];
	static u64 query_count = 0;
	static s64   selected     = -1;
	static struct nk_scroll scroll = {0};
	static bool query_done = true;
	
	
	nk_layout_row_dynamic(ctx, 30, 1);
	nk_label(ctx, "Directory:", NK_TEXT_LEFT);
	nk_edit_string_zero_terminated(ctx, NK_EDIT_SIMPLE,
		dir, MAX_PATH, nk_filter_default);
	
	nk_layout_row_dynamic(ctx, 30, 1);
	nk_label(ctx, "Filter:", NK_TEXT_LEFT);
	nk_edit_string_zero_terminated(ctx, NK_EDIT_SIMPLE,
		filter, MAX_FILTER, nk_filter_default);
	
	nk_layout_row_dynamic(ctx, 30, 1);
	nk_label(ctx, "Search term:", NK_TEXT_LEFT);
	nk_edit_string_zero_terminated(ctx, NK_EDIT_SIMPLE,
		search_str, MAX_SEARCH, nk_filter_default);
	
	search_term.data = (u8*)search_str;
	search_term.count = c_style_strlen(search_str);
	
	nk_layout_row_dynamic(ctx, 30, 2);
	if (nk_button_label(ctx, "Search")) {
		
		for (u64 i = 0; i < query_count; i += 1) {
			query_free(&queries[i]);
		}
		query_count = 0;
		
		string sdir = STR(dir);
		
		if (sdir.count == 2 && sdir.data[1] == ':') {
			sdir = tprint("%s\\", sdir);
		}
		
		sys_get_absolute_path(get_temp(), sdir, &sdir);
		
		s64 dir_colon_index = string_find_index_from_left(sdir, STR(":"));
		
		bool one_volume = dir_colon_index != -1;
		
		for (u64 i = 0; i < state.vol_count; i += 1) {
			Volume_Database vol = state.vol_states[i].vol;
			
			if (one_volume) {
				
				if (strings_match(string_slice(sdir, 0, (u64)dir_colon_index), vol.name)) {
					
					Entry *search_directory_entry = full_path_to_entry_directory(vol, sdir);
					if (!search_directory_entry) {
						print("Could not find search directory entry\n");
						continue;
					}
					
					string filters[2048];
					u64 filter_count = parse_filters(STR(filter), filters, 2048);
					
					u64 search_term_count = 1;
					
					if (search_term.count == 0 || strings_match(search_term, STR("*"))) {
						search_term_count = 0;
					}
					Query *q = &queries[query_count++];
					query_volume(q, vol, search_directory_entry, filters, filter_count, &search_term, search_term_count, &query_done);
					
					print_stats(vol, queries[query_count-1]);
					timestamp_count = 0;
					
					break;
				}
				
			}
		}
		
		
		selected = -1;
	}
	if (nk_button_label(ctx, "Clear")) {
		for (u64 i = 0; i < query_count; i += 1) {
			query_free(&queries[i]);
		}
		query_count = 0;
		selected     = -1;
	}
	if (nk_button_label(ctx, "debugbreak")) {
		__debugbreak();
	}
	
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
	
	u64 files_searched = 0;
	u64 file_count = 0;
	u64 matches_found = 0;
	u64 unique_file_count = 0;
	f64 query_time = 0.0;
	
	if (query_count > 0) {
		Query *query = &queries[0];
		
		for (u64 i = 0; i < persistent_array_count(query->filtered_entries); i += 1) {
			if (query->filtered_entries[i]->was_searched) {
				files_searched += 1;
			}
		}
		
		file_count = persistent_array_count(query->filtered_entries);
		
		matches_found = persistent_array_count(query->matches);
		if (!matches_found) matches_found = persistent_array_count(query->filtered_entries);
		
		unique_file_count = query->unique_files_with_any_match_count;
		
		query_time = query_done ? query->end_time - query->start_time : sys_get_seconds_monotonic() - query->start_time;
		
		
	}
	string s = query_done ? STR("Done") : STR("Querying...");
	string query_state_text = tprint("%s. (%f s) Files searched: %u/%u, Matches found: %u, in %u files.", s, query_time, files_searched, file_count, matches_found, unique_file_count);
	nk_text(ctx, (char*)query_state_text.data, (int)query_state_text.count, NK_TEXT_LEFT);
	
	struct nk_rect region = nk_window_get_content_region(ctx);
	struct nk_rect last = nk_widget_bounds(ctx);
	float used = (last.y + last.h) - region.y;
	float remaining = region.h - used;
	
	nk_layout_row_dynamic(ctx, remaining, 1);
	if (nk_group_scrolled_begin(ctx,
		&scroll,
		"Results",
		NK_WINDOW_BORDER))
	{
		if (query_count > 0) {
			Query *query = &queries[0];
			
			
			string open_command = tprint("explorer.exe /select,");
			
			// Look for a good file explorer. If none is found, just use windows explorer.
			char dummy[2048];
			DWORD result = SearchPathA(0, "FPilot.exe", 0, 2048, dummy, 0);
			if (result > 0) {
				open_command = tprint("FPilot.exe ");
			}
			
			if (persistent_array_count(query->matches) == 0) {
				nk_layout_row_dynamic(ctx, 20, 1);
				for (u64 j = 0; j < persistent_array_count(query->filtered_entries); ++j) {
					Entry *entry = query->filtered_entries[j];
					string name = tprint_entry_full_path(query->vol.name, entry);
					nk_bool is_sel = (selected == (s64)j);
					if (nk_selectable_text(ctx, (char*)name.data, (int)name.count, NK_TEXT_LEFT, &is_sel)) {
						if (selected == (s64)j) {
							sys_run_command_easy(tprint("%s\"%s\"", open_command, name), sys_get_stdout(), sys_get_stderr(), STR("."), false);
						}
						selected = (s64)j;
					}
				}
			} else {
				nk_layout_row_dynamic(ctx, 20, 2);
				for (u64 j = 0; j < persistent_array_count(query->matches); ++j) {
					Match match = query->matches[j];
					Entry *entry = match.entry;
					string name = tprint_entry_full_path(query->vol.name, entry);
					nk_bool is_sel = (selected == (s64)j);
					if (nk_selectable_text(ctx, (char*)name.data, (int)name.count, NK_TEXT_LEFT, &is_sel)) {
						if (selected == (s64)j) {
							sys_run_command_easy(tprint("%s\"%s\"", open_command, name), sys_get_stdout(), sys_get_stderr(), STR("."), false);
						}
						selected = (s64)j;
					}
					nk_text(ctx, (char*)match.line.data, (int)match.line.count, NK_TEXT_LEFT);
				}
				for (u64 j = 0; j < persistent_array_count(query->filtered_entries); ++j) {
					Entry *entry = query->filtered_entries[j];
					if (!entry->was_searched) {
						string name = tprint_entry_full_path(query->vol.name, entry);
						nk_bool is_sel = (selected == (s64)j);
						if (nk_selectable_text(ctx, (char*)name.data, (int)name.count, NK_TEXT_LEFT, &is_sel)) {
							if (selected == (s64)j) {
								sys_run_command_easy(tprint("%s\"%s\"", open_command, name), sys_get_stdout(), sys_get_stderr(), STR("."), false);
							}
							selected = (s64)j;
						}
						if (entry->data == 0) {
							nk_text(ctx, "<Dataless>", (int)c_style_strlen("<Dataless>"), NK_TEXT_LEFT);
						} else if (!entry->valid) {
							nk_text(ctx, "<Invalid>", (int)c_style_strlen("<Invalid>"), NK_TEXT_LEFT);
						} else {
							nk_text(ctx, "<Queued for searching...>", (int)c_style_strlen("<Queued for searching...>"), NK_TEXT_LEFT);
						}
					}
				}
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
