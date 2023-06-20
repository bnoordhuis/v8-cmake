#!/usr/bin/env python3
# Copyright 2019 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

# This file is automatically generated by mkgrokdump and should not
# be modified manually.

# List of known V8 instance types.
# yapf: disable

INSTANCE_TYPES = {
  0: "INTERNALIZED_STRING_TYPE",
  2: "EXTERNAL_INTERNALIZED_STRING_TYPE",
  8: "ONE_BYTE_INTERNALIZED_STRING_TYPE",
  10: "EXTERNAL_ONE_BYTE_INTERNALIZED_STRING_TYPE",
  18: "UNCACHED_EXTERNAL_INTERNALIZED_STRING_TYPE",
  26: "UNCACHED_EXTERNAL_ONE_BYTE_INTERNALIZED_STRING_TYPE",
  32: "STRING_TYPE",
  33: "CONS_STRING_TYPE",
  34: "EXTERNAL_STRING_TYPE",
  35: "SLICED_STRING_TYPE",
  37: "THIN_STRING_TYPE",
  40: "ONE_BYTE_STRING_TYPE",
  41: "CONS_ONE_BYTE_STRING_TYPE",
  42: "EXTERNAL_ONE_BYTE_STRING_TYPE",
  43: "SLICED_ONE_BYTE_STRING_TYPE",
  50: "UNCACHED_EXTERNAL_STRING_TYPE",
  58: "UNCACHED_EXTERNAL_ONE_BYTE_STRING_TYPE",
  96: "SHARED_STRING_TYPE",
  98: "SHARED_EXTERNAL_STRING_TYPE",
  104: "SHARED_ONE_BYTE_STRING_TYPE",
  106: "SHARED_EXTERNAL_ONE_BYTE_STRING_TYPE",
  114: "SHARED_UNCACHED_EXTERNAL_STRING_TYPE",
  122: "SHARED_UNCACHED_EXTERNAL_ONE_BYTE_STRING_TYPE",
  128: "SYMBOL_TYPE",
  129: "BIG_INT_BASE_TYPE",
  130: "HEAP_NUMBER_TYPE",
  131: "ODDBALL_TYPE",
  132: "PROMISE_FULFILL_REACTION_JOB_TASK_TYPE",
  133: "PROMISE_REJECT_REACTION_JOB_TASK_TYPE",
  134: "CALLABLE_TASK_TYPE",
  135: "CALLBACK_TASK_TYPE",
  136: "PROMISE_RESOLVE_THENABLE_JOB_TASK_TYPE",
  137: "LOAD_HANDLER_TYPE",
  138: "STORE_HANDLER_TYPE",
  139: "FUNCTION_TEMPLATE_INFO_TYPE",
  140: "OBJECT_TEMPLATE_INFO_TYPE",
  141: "ACCESS_CHECK_INFO_TYPE",
  142: "ACCESSOR_PAIR_TYPE",
  143: "ALIASED_ARGUMENTS_ENTRY_TYPE",
  144: "ALLOCATION_MEMENTO_TYPE",
  145: "ALLOCATION_SITE_TYPE",
  146: "ARRAY_BOILERPLATE_DESCRIPTION_TYPE",
  147: "ASM_WASM_DATA_TYPE",
  148: "ASYNC_GENERATOR_REQUEST_TYPE",
  149: "BREAK_POINT_TYPE",
  150: "BREAK_POINT_INFO_TYPE",
  151: "CALL_SITE_INFO_TYPE",
  152: "CLASS_POSITIONS_TYPE",
  153: "DEBUG_INFO_TYPE",
  154: "ENUM_CACHE_TYPE",
  155: "ERROR_STACK_DATA_TYPE",
  156: "FEEDBACK_CELL_TYPE",
  157: "FUNCTION_TEMPLATE_RARE_DATA_TYPE",
  158: "INTERCEPTOR_INFO_TYPE",
  159: "INTERPRETER_DATA_TYPE",
  160: "MODULE_REQUEST_TYPE",
  161: "PROMISE_CAPABILITY_TYPE",
  162: "PROMISE_ON_STACK_TYPE",
  163: "PROMISE_REACTION_TYPE",
  164: "PROPERTY_DESCRIPTOR_OBJECT_TYPE",
  165: "PROTOTYPE_INFO_TYPE",
  166: "REG_EXP_BOILERPLATE_DESCRIPTION_TYPE",
  167: "SCRIPT_TYPE",
  168: "SCRIPT_OR_MODULE_TYPE",
  169: "SOURCE_TEXT_MODULE_INFO_ENTRY_TYPE",
  170: "STACK_FRAME_INFO_TYPE",
  171: "TEMPLATE_OBJECT_DESCRIPTION_TYPE",
  172: "TUPLE2_TYPE",
  173: "WASM_EXCEPTION_TAG_TYPE",
  174: "WASM_INDIRECT_FUNCTION_TABLE_TYPE",
  175: "FIXED_ARRAY_TYPE",
  176: "HASH_TABLE_TYPE",
  177: "EPHEMERON_HASH_TABLE_TYPE",
  178: "GLOBAL_DICTIONARY_TYPE",
  179: "NAME_DICTIONARY_TYPE",
  180: "NAME_TO_INDEX_HASH_TABLE_TYPE",
  181: "NUMBER_DICTIONARY_TYPE",
  182: "ORDERED_HASH_MAP_TYPE",
  183: "ORDERED_HASH_SET_TYPE",
  184: "ORDERED_NAME_DICTIONARY_TYPE",
  185: "REGISTERED_SYMBOL_TABLE_TYPE",
  186: "SIMPLE_NUMBER_DICTIONARY_TYPE",
  187: "CLOSURE_FEEDBACK_CELL_ARRAY_TYPE",
  188: "OBJECT_BOILERPLATE_DESCRIPTION_TYPE",
  189: "SCRIPT_CONTEXT_TABLE_TYPE",
  190: "BYTE_ARRAY_TYPE",
  191: "BYTECODE_ARRAY_TYPE",
  192: "FIXED_DOUBLE_ARRAY_TYPE",
  193: "INTERNAL_CLASS_WITH_SMI_ELEMENTS_TYPE",
  194: "SLOPPY_ARGUMENTS_ELEMENTS_TYPE",
  195: "TURBOSHAFT_FLOAT64_TYPE_TYPE",
  196: "TURBOSHAFT_FLOAT64_RANGE_TYPE_TYPE",
  197: "TURBOSHAFT_FLOAT64_SET_TYPE_TYPE",
  198: "TURBOSHAFT_WORD32_TYPE_TYPE",
  199: "TURBOSHAFT_WORD32_RANGE_TYPE_TYPE",
  200: "TURBOSHAFT_WORD32_SET_TYPE_TYPE",
  201: "TURBOSHAFT_WORD64_TYPE_TYPE",
  202: "TURBOSHAFT_WORD64_RANGE_TYPE_TYPE",
  203: "TURBOSHAFT_WORD64_SET_TYPE_TYPE",
  204: "FOREIGN_TYPE",
  205: "AWAIT_CONTEXT_TYPE",
  206: "BLOCK_CONTEXT_TYPE",
  207: "CATCH_CONTEXT_TYPE",
  208: "DEBUG_EVALUATE_CONTEXT_TYPE",
  209: "EVAL_CONTEXT_TYPE",
  210: "FUNCTION_CONTEXT_TYPE",
  211: "MODULE_CONTEXT_TYPE",
  212: "NATIVE_CONTEXT_TYPE",
  213: "SCRIPT_CONTEXT_TYPE",
  214: "WITH_CONTEXT_TYPE",
  215: "TURBOFAN_BITSET_TYPE_TYPE",
  216: "TURBOFAN_HEAP_CONSTANT_TYPE_TYPE",
  217: "TURBOFAN_OTHER_NUMBER_CONSTANT_TYPE_TYPE",
  218: "TURBOFAN_RANGE_TYPE_TYPE",
  219: "TURBOFAN_UNION_TYPE_TYPE",
  220: "UNCOMPILED_DATA_WITH_PREPARSE_DATA_TYPE",
  221: "UNCOMPILED_DATA_WITH_PREPARSE_DATA_AND_JOB_TYPE",
  222: "UNCOMPILED_DATA_WITHOUT_PREPARSE_DATA_TYPE",
  223: "UNCOMPILED_DATA_WITHOUT_PREPARSE_DATA_WITH_JOB_TYPE",
  224: "WASM_FUNCTION_DATA_TYPE",
  225: "WASM_CAPI_FUNCTION_DATA_TYPE",
  226: "WASM_EXPORTED_FUNCTION_DATA_TYPE",
  227: "WASM_JS_FUNCTION_DATA_TYPE",
  228: "EXPORTED_SUB_CLASS_BASE_TYPE",
  229: "EXPORTED_SUB_CLASS_TYPE",
  230: "EXPORTED_SUB_CLASS2_TYPE",
  231: "SMALL_ORDERED_HASH_MAP_TYPE",
  232: "SMALL_ORDERED_HASH_SET_TYPE",
  233: "SMALL_ORDERED_NAME_DICTIONARY_TYPE",
  234: "ABSTRACT_INTERNAL_CLASS_SUBCLASS1_TYPE",
  235: "ABSTRACT_INTERNAL_CLASS_SUBCLASS2_TYPE",
  236: "DESCRIPTOR_ARRAY_TYPE",
  237: "STRONG_DESCRIPTOR_ARRAY_TYPE",
  238: "SOURCE_TEXT_MODULE_TYPE",
  239: "SYNTHETIC_MODULE_TYPE",
  240: "WEAK_FIXED_ARRAY_TYPE",
  241: "TRANSITION_ARRAY_TYPE",
  242: "ACCESSOR_INFO_TYPE",
  243: "CALL_HANDLER_INFO_TYPE",
  244: "CELL_TYPE",
  245: "CODE_TYPE",
  246: "COVERAGE_INFO_TYPE",
  247: "EMBEDDER_DATA_ARRAY_TYPE",
  248: "FEEDBACK_METADATA_TYPE",
  249: "FEEDBACK_VECTOR_TYPE",
  250: "FILLER_TYPE",
  251: "FREE_SPACE_TYPE",
  252: "HOLE_TYPE",
  253: "INSTRUCTION_STREAM_TYPE",
  254: "INTERNAL_CLASS_TYPE",
  255: "INTERNAL_CLASS_WITH_STRUCT_ELEMENTS_TYPE",
  256: "MAP_TYPE",
  257: "MEGA_DOM_HANDLER_TYPE",
  258: "ON_HEAP_BASIC_BLOCK_PROFILER_DATA_TYPE",
  259: "PREPARSE_DATA_TYPE",
  260: "PROPERTY_ARRAY_TYPE",
  261: "PROPERTY_CELL_TYPE",
  262: "SCOPE_INFO_TYPE",
  263: "SHARED_FUNCTION_INFO_TYPE",
  264: "SMI_BOX_TYPE",
  265: "SMI_PAIR_TYPE",
  266: "SORT_STATE_TYPE",
  267: "SWISS_NAME_DICTIONARY_TYPE",
  268: "WASM_API_FUNCTION_REF_TYPE",
  269: "WASM_CONTINUATION_OBJECT_TYPE",
  270: "WASM_INTERNAL_FUNCTION_TYPE",
  271: "WASM_NULL_TYPE",
  272: "WASM_RESUME_DATA_TYPE",
  273: "WASM_STRING_VIEW_ITER_TYPE",
  274: "WASM_TYPE_INFO_TYPE",
  275: "WEAK_ARRAY_LIST_TYPE",
  276: "WEAK_CELL_TYPE",
  277: "WASM_ARRAY_TYPE",
  278: "WASM_STRUCT_TYPE",
  279: "JS_PROXY_TYPE",
  1057: "JS_OBJECT_TYPE",
  280: "JS_GLOBAL_OBJECT_TYPE",
  281: "JS_GLOBAL_PROXY_TYPE",
  282: "JS_MODULE_NAMESPACE_TYPE",
  1040: "JS_SPECIAL_API_OBJECT_TYPE",
  1041: "JS_PRIMITIVE_WRAPPER_TYPE",
  1058: "JS_API_OBJECT_TYPE",
  2058: "JS_LAST_DUMMY_API_OBJECT_TYPE",
  2059: "JS_DATA_VIEW_TYPE",
  2060: "JS_RAB_GSAB_DATA_VIEW_TYPE",
  2061: "JS_TYPED_ARRAY_TYPE",
  2062: "JS_ARRAY_BUFFER_TYPE",
  2063: "JS_PROMISE_TYPE",
  2064: "JS_BOUND_FUNCTION_TYPE",
  2065: "JS_WRAPPED_FUNCTION_TYPE",
  2066: "JS_FUNCTION_TYPE",
  2067: "BIGINT64_TYPED_ARRAY_CONSTRUCTOR_TYPE",
  2068: "BIGUINT64_TYPED_ARRAY_CONSTRUCTOR_TYPE",
  2069: "FLOAT32_TYPED_ARRAY_CONSTRUCTOR_TYPE",
  2070: "FLOAT64_TYPED_ARRAY_CONSTRUCTOR_TYPE",
  2071: "INT16_TYPED_ARRAY_CONSTRUCTOR_TYPE",
  2072: "INT32_TYPED_ARRAY_CONSTRUCTOR_TYPE",
  2073: "INT8_TYPED_ARRAY_CONSTRUCTOR_TYPE",
  2074: "UINT16_TYPED_ARRAY_CONSTRUCTOR_TYPE",
  2075: "UINT32_TYPED_ARRAY_CONSTRUCTOR_TYPE",
  2076: "UINT8_CLAMPED_TYPED_ARRAY_CONSTRUCTOR_TYPE",
  2077: "UINT8_TYPED_ARRAY_CONSTRUCTOR_TYPE",
  2078: "JS_ARRAY_CONSTRUCTOR_TYPE",
  2079: "JS_PROMISE_CONSTRUCTOR_TYPE",
  2080: "JS_REG_EXP_CONSTRUCTOR_TYPE",
  2081: "JS_CLASS_CONSTRUCTOR_TYPE",
  2082: "JS_ARRAY_ITERATOR_PROTOTYPE_TYPE",
  2083: "JS_ITERATOR_PROTOTYPE_TYPE",
  2084: "JS_MAP_ITERATOR_PROTOTYPE_TYPE",
  2085: "JS_OBJECT_PROTOTYPE_TYPE",
  2086: "JS_PROMISE_PROTOTYPE_TYPE",
  2087: "JS_REG_EXP_PROTOTYPE_TYPE",
  2088: "JS_SET_ITERATOR_PROTOTYPE_TYPE",
  2089: "JS_SET_PROTOTYPE_TYPE",
  2090: "JS_STRING_ITERATOR_PROTOTYPE_TYPE",
  2091: "JS_TYPED_ARRAY_PROTOTYPE_TYPE",
  2092: "JS_MAP_KEY_ITERATOR_TYPE",
  2093: "JS_MAP_KEY_VALUE_ITERATOR_TYPE",
  2094: "JS_MAP_VALUE_ITERATOR_TYPE",
  2095: "JS_SET_KEY_VALUE_ITERATOR_TYPE",
  2096: "JS_SET_VALUE_ITERATOR_TYPE",
  2097: "JS_ITERATOR_DROP_HELPER_TYPE",
  2098: "JS_ITERATOR_FILTER_HELPER_TYPE",
  2099: "JS_ITERATOR_FLAT_MAP_HELPER_TYPE",
  2100: "JS_ITERATOR_MAP_HELPER_TYPE",
  2101: "JS_ITERATOR_TAKE_HELPER_TYPE",
  2102: "JS_ATOMICS_CONDITION_TYPE",
  2103: "JS_ATOMICS_MUTEX_TYPE",
  2104: "JS_SHARED_ARRAY_TYPE",
  2105: "JS_SHARED_STRUCT_TYPE",
  2106: "JS_GENERATOR_OBJECT_TYPE",
  2107: "JS_ASYNC_FUNCTION_OBJECT_TYPE",
  2108: "JS_ASYNC_GENERATOR_OBJECT_TYPE",
  2109: "JS_MAP_TYPE",
  2110: "JS_SET_TYPE",
  2111: "JS_WEAK_MAP_TYPE",
  2112: "JS_WEAK_SET_TYPE",
  2113: "JS_ARGUMENTS_OBJECT_TYPE",
  2114: "JS_ARRAY_TYPE",
  2115: "JS_ARRAY_ITERATOR_TYPE",
  2116: "JS_ASYNC_FROM_SYNC_ITERATOR_TYPE",
  2117: "JS_COLLATOR_TYPE",
  2118: "JS_CONTEXT_EXTENSION_OBJECT_TYPE",
  2119: "JS_DATE_TYPE",
  2120: "JS_DATE_TIME_FORMAT_TYPE",
  2121: "JS_DISPLAY_NAMES_TYPE",
  2122: "JS_DURATION_FORMAT_TYPE",
  2123: "JS_ERROR_TYPE",
  2124: "JS_EXTERNAL_OBJECT_TYPE",
  2125: "JS_FINALIZATION_REGISTRY_TYPE",
  2126: "JS_LIST_FORMAT_TYPE",
  2127: "JS_LOCALE_TYPE",
  2128: "JS_MESSAGE_OBJECT_TYPE",
  2129: "JS_NUMBER_FORMAT_TYPE",
  2130: "JS_PLURAL_RULES_TYPE",
  2131: "JS_RAW_JSON_TYPE",
  2132: "JS_REG_EXP_TYPE",
  2133: "JS_REG_EXP_STRING_ITERATOR_TYPE",
  2134: "JS_RELATIVE_TIME_FORMAT_TYPE",
  2135: "JS_SEGMENT_ITERATOR_TYPE",
  2136: "JS_SEGMENTER_TYPE",
  2137: "JS_SEGMENTS_TYPE",
  2138: "JS_SHADOW_REALM_TYPE",
  2139: "JS_STRING_ITERATOR_TYPE",
  2140: "JS_TEMPORAL_CALENDAR_TYPE",
  2141: "JS_TEMPORAL_DURATION_TYPE",
  2142: "JS_TEMPORAL_INSTANT_TYPE",
  2143: "JS_TEMPORAL_PLAIN_DATE_TYPE",
  2144: "JS_TEMPORAL_PLAIN_DATE_TIME_TYPE",
  2145: "JS_TEMPORAL_PLAIN_MONTH_DAY_TYPE",
  2146: "JS_TEMPORAL_PLAIN_TIME_TYPE",
  2147: "JS_TEMPORAL_PLAIN_YEAR_MONTH_TYPE",
  2148: "JS_TEMPORAL_TIME_ZONE_TYPE",
  2149: "JS_TEMPORAL_ZONED_DATE_TIME_TYPE",
  2150: "JS_V8_BREAK_ITERATOR_TYPE",
  2151: "JS_VALID_ITERATOR_WRAPPER_TYPE",
  2152: "JS_WEAK_REF_TYPE",
  2153: "WASM_EXCEPTION_PACKAGE_TYPE",
  2154: "WASM_GLOBAL_OBJECT_TYPE",
  2155: "WASM_INSTANCE_OBJECT_TYPE",
  2156: "WASM_MEMORY_OBJECT_TYPE",
  2157: "WASM_MODULE_OBJECT_TYPE",
  2158: "WASM_SUSPENDER_OBJECT_TYPE",
  2159: "WASM_TABLE_OBJECT_TYPE",
  2160: "WASM_TAG_OBJECT_TYPE",
  2161: "WASM_VALUE_OBJECT_TYPE",
}

# List of known V8 maps.
KNOWN_MAPS = {
    ("read_only_space", 0x00061): (256, "MetaMap"),
    ("read_only_space", 0x00089): (175, "FixedArrayMap"),
    ("read_only_space", 0x000b1): (240, "WeakFixedArrayMap"),
    ("read_only_space", 0x000d9): (275, "WeakArrayListMap"),
    ("read_only_space", 0x00101): (175, "FixedCOWArrayMap"),
    ("read_only_space", 0x00129): (236, "DescriptorArrayMap"),
    ("read_only_space", 0x00151): (131, "UndefinedMap"),
    ("read_only_space", 0x00179): (131, "NullMap"),
    ("read_only_space", 0x001a1): (252, "TheHoleMap"),
    ("read_only_space", 0x001c9): (151, "CallSiteInfoMap"),
    ("read_only_space", 0x001f1): (154, "EnumCacheMap"),
    ("read_only_space", 0x00299): (262, "ScopeInfoMap"),
    ("read_only_space", 0x002c1): (175, "ModuleInfoMap"),
    ("read_only_space", 0x002e9): (187, "ClosureFeedbackCellArrayMap"),
    ("read_only_space", 0x00311): (249, "FeedbackVectorMap"),
    ("read_only_space", 0x00339): (130, "HeapNumberMap"),
    ("read_only_space", 0x00361): (204, "ForeignMap"),
    ("read_only_space", 0x00389): (257, "MegaDomHandlerMap"),
    ("read_only_space", 0x003b1): (131, "BooleanMap"),
    ("read_only_space", 0x003d9): (131, "UninitializedMap"),
    ("read_only_space", 0x00401): (131, "ArgumentsMarkerMap"),
    ("read_only_space", 0x00429): (131, "ExceptionMap"),
    ("read_only_space", 0x00451): (131, "TerminationExceptionMap"),
    ("read_only_space", 0x00479): (131, "OptimizedOutMap"),
    ("read_only_space", 0x004a1): (131, "StaleRegisterMap"),
    ("read_only_space", 0x004c9): (131, "SelfReferenceMarkerMap"),
    ("read_only_space", 0x004f1): (131, "BasicBlockCountersMarkerMap"),
    ("read_only_space", 0x00519): (129, "BigIntMap"),
    ("read_only_space", 0x00541): (128, "SymbolMap"),
    ("read_only_space", 0x00569): (32, "StringMap"),
    ("read_only_space", 0x00591): (40, "OneByteStringMap"),
    ("read_only_space", 0x005b9): (33, "ConsStringMap"),
    ("read_only_space", 0x005e1): (41, "ConsOneByteStringMap"),
    ("read_only_space", 0x00609): (35, "SlicedStringMap"),
    ("read_only_space", 0x00631): (43, "SlicedOneByteStringMap"),
    ("read_only_space", 0x00659): (34, "ExternalStringMap"),
    ("read_only_space", 0x00681): (42, "ExternalOneByteStringMap"),
    ("read_only_space", 0x006a9): (50, "UncachedExternalStringMap"),
    ("read_only_space", 0x006d1): (58, "UncachedExternalOneByteStringMap"),
    ("read_only_space", 0x006f9): (98, "SharedExternalStringMap"),
    ("read_only_space", 0x00721): (106, "SharedExternalOneByteStringMap"),
    ("read_only_space", 0x00749): (114, "SharedUncachedExternalStringMap"),
    ("read_only_space", 0x00771): (122, "SharedUncachedExternalOneByteStringMap"),
    ("read_only_space", 0x00799): (2, "ExternalInternalizedStringMap"),
    ("read_only_space", 0x007c1): (10, "ExternalOneByteInternalizedStringMap"),
    ("read_only_space", 0x007e9): (18, "UncachedExternalInternalizedStringMap"),
    ("read_only_space", 0x00811): (26, "UncachedExternalOneByteInternalizedStringMap"),
    ("read_only_space", 0x00839): (0, "InternalizedStringMap"),
    ("read_only_space", 0x00861): (8, "OneByteInternalizedStringMap"),
    ("read_only_space", 0x00889): (37, "ThinStringMap"),
    ("read_only_space", 0x008b1): (96, "SharedStringMap"),
    ("read_only_space", 0x008d9): (104, "SharedOneByteStringMap"),
    ("read_only_space", 0x00901): (192, "FixedDoubleArrayMap"),
    ("read_only_space", 0x00929): (248, "FeedbackMetadataArrayMap"),
    ("read_only_space", 0x00951): (190, "ByteArrayMap"),
    ("read_only_space", 0x00979): (191, "BytecodeArrayMap"),
    ("read_only_space", 0x009a1): (251, "FreeSpaceMap"),
    ("read_only_space", 0x009c9): (260, "PropertyArrayMap"),
    ("read_only_space", 0x009f1): (231, "SmallOrderedHashMapMap"),
    ("read_only_space", 0x00a19): (232, "SmallOrderedHashSetMap"),
    ("read_only_space", 0x00a41): (233, "SmallOrderedNameDictionaryMap"),
    ("read_only_space", 0x00a69): (253, "InstructionStreamMap"),
    ("read_only_space", 0x00a91): (244, "CellMap"),
    ("read_only_space", 0x00ac1): (261, "GlobalPropertyCellMap"),
    ("read_only_space", 0x00ae9): (250, "OnePointerFillerMap"),
    ("read_only_space", 0x00b11): (250, "TwoPointerFillerMap"),
    ("read_only_space", 0x00b39): (156, "NoClosuresCellMap"),
    ("read_only_space", 0x00b61): (156, "OneClosureCellMap"),
    ("read_only_space", 0x00b89): (156, "ManyClosuresCellMap"),
    ("read_only_space", 0x00bb1): (241, "TransitionArrayMap"),
    ("read_only_space", 0x00bd9): (176, "HashTableMap"),
    ("read_only_space", 0x00c01): (184, "OrderedNameDictionaryMap"),
    ("read_only_space", 0x00c29): (179, "NameDictionaryMap"),
    ("read_only_space", 0x00c51): (267, "SwissNameDictionaryMap"),
    ("read_only_space", 0x00c79): (178, "GlobalDictionaryMap"),
    ("read_only_space", 0x00ca1): (181, "NumberDictionaryMap"),
    ("read_only_space", 0x00cc9): (185, "RegisteredSymbolTableMap"),
    ("read_only_space", 0x00cf1): (175, "ArrayListMap"),
    ("read_only_space", 0x00d19): (242, "AccessorInfoMap"),
    ("read_only_space", 0x00d41): (259, "PreparseDataMap"),
    ("read_only_space", 0x00d69): (263, "SharedFunctionInfoMap"),
    ("read_only_space", 0x00d91): (245, "CodeMap"),
    ("read_only_space", 0x00fed): (132, "PromiseFulfillReactionJobTaskMap"),
    ("read_only_space", 0x01015): (133, "PromiseRejectReactionJobTaskMap"),
    ("read_only_space", 0x0103d): (134, "CallableTaskMap"),
    ("read_only_space", 0x01065): (135, "CallbackTaskMap"),
    ("read_only_space", 0x0108d): (136, "PromiseResolveThenableJobTaskMap"),
    ("read_only_space", 0x010b5): (139, "FunctionTemplateInfoMap"),
    ("read_only_space", 0x010dd): (140, "ObjectTemplateInfoMap"),
    ("read_only_space", 0x01105): (141, "AccessCheckInfoMap"),
    ("read_only_space", 0x0112d): (142, "AccessorPairMap"),
    ("read_only_space", 0x01155): (143, "AliasedArgumentsEntryMap"),
    ("read_only_space", 0x0117d): (144, "AllocationMementoMap"),
    ("read_only_space", 0x011a5): (146, "ArrayBoilerplateDescriptionMap"),
    ("read_only_space", 0x011cd): (147, "AsmWasmDataMap"),
    ("read_only_space", 0x011f5): (148, "AsyncGeneratorRequestMap"),
    ("read_only_space", 0x0121d): (149, "BreakPointMap"),
    ("read_only_space", 0x01245): (150, "BreakPointInfoMap"),
    ("read_only_space", 0x0126d): (152, "ClassPositionsMap"),
    ("read_only_space", 0x01295): (153, "DebugInfoMap"),
    ("read_only_space", 0x012bd): (155, "ErrorStackDataMap"),
    ("read_only_space", 0x012e5): (157, "FunctionTemplateRareDataMap"),
    ("read_only_space", 0x0130d): (158, "InterceptorInfoMap"),
    ("read_only_space", 0x01335): (159, "InterpreterDataMap"),
    ("read_only_space", 0x0135d): (160, "ModuleRequestMap"),
    ("read_only_space", 0x01385): (161, "PromiseCapabilityMap"),
    ("read_only_space", 0x013ad): (162, "PromiseOnStackMap"),
    ("read_only_space", 0x013d5): (163, "PromiseReactionMap"),
    ("read_only_space", 0x013fd): (164, "PropertyDescriptorObjectMap"),
    ("read_only_space", 0x01425): (165, "PrototypeInfoMap"),
    ("read_only_space", 0x0144d): (166, "RegExpBoilerplateDescriptionMap"),
    ("read_only_space", 0x01475): (167, "ScriptMap"),
    ("read_only_space", 0x0149d): (168, "ScriptOrModuleMap"),
    ("read_only_space", 0x014c5): (169, "SourceTextModuleInfoEntryMap"),
    ("read_only_space", 0x014ed): (170, "StackFrameInfoMap"),
    ("read_only_space", 0x01515): (171, "TemplateObjectDescriptionMap"),
    ("read_only_space", 0x0153d): (172, "Tuple2Map"),
    ("read_only_space", 0x01565): (173, "WasmExceptionTagMap"),
    ("read_only_space", 0x0158d): (174, "WasmIndirectFunctionTableMap"),
    ("read_only_space", 0x015b5): (145, "AllocationSiteWithWeakNextMap"),
    ("read_only_space", 0x015dd): (145, "AllocationSiteWithoutWeakNextMap"),
    ("read_only_space", 0x01605): (137, "LoadHandler1Map"),
    ("read_only_space", 0x0162d): (137, "LoadHandler2Map"),
    ("read_only_space", 0x01655): (137, "LoadHandler3Map"),
    ("read_only_space", 0x0167d): (138, "StoreHandler0Map"),
    ("read_only_space", 0x016a5): (138, "StoreHandler1Map"),
    ("read_only_space", 0x016cd): (138, "StoreHandler2Map"),
    ("read_only_space", 0x016f5): (138, "StoreHandler3Map"),
    ("read_only_space", 0x0171d): (222, "UncompiledDataWithoutPreparseDataMap"),
    ("read_only_space", 0x01745): (220, "UncompiledDataWithPreparseDataMap"),
    ("read_only_space", 0x0176d): (223, "UncompiledDataWithoutPreparseDataWithJobMap"),
    ("read_only_space", 0x01795): (221, "UncompiledDataWithPreparseDataAndJobMap"),
    ("read_only_space", 0x017bd): (258, "OnHeapBasicBlockProfilerDataMap"),
    ("read_only_space", 0x017e5): (215, "TurbofanBitsetTypeMap"),
    ("read_only_space", 0x0180d): (219, "TurbofanUnionTypeMap"),
    ("read_only_space", 0x01835): (218, "TurbofanRangeTypeMap"),
    ("read_only_space", 0x0185d): (216, "TurbofanHeapConstantTypeMap"),
    ("read_only_space", 0x01885): (217, "TurbofanOtherNumberConstantTypeMap"),
    ("read_only_space", 0x018ad): (198, "TurboshaftWord32TypeMap"),
    ("read_only_space", 0x018d5): (199, "TurboshaftWord32RangeTypeMap"),
    ("read_only_space", 0x018fd): (201, "TurboshaftWord64TypeMap"),
    ("read_only_space", 0x01925): (202, "TurboshaftWord64RangeTypeMap"),
    ("read_only_space", 0x0194d): (195, "TurboshaftFloat64TypeMap"),
    ("read_only_space", 0x01975): (196, "TurboshaftFloat64RangeTypeMap"),
    ("read_only_space", 0x0199d): (254, "InternalClassMap"),
    ("read_only_space", 0x019c5): (265, "SmiPairMap"),
    ("read_only_space", 0x019ed): (264, "SmiBoxMap"),
    ("read_only_space", 0x01a15): (228, "ExportedSubClassBaseMap"),
    ("read_only_space", 0x01a3d): (229, "ExportedSubClassMap"),
    ("read_only_space", 0x01a65): (234, "AbstractInternalClassSubclass1Map"),
    ("read_only_space", 0x01a8d): (235, "AbstractInternalClassSubclass2Map"),
    ("read_only_space", 0x01ab5): (230, "ExportedSubClass2Map"),
    ("read_only_space", 0x01add): (266, "SortStateMap"),
    ("read_only_space", 0x01b05): (273, "WasmStringViewIterMap"),
    ("read_only_space", 0x01b2d): (194, "SloppyArgumentsElementsMap"),
    ("read_only_space", 0x01b55): (237, "StrongDescriptorArrayMap"),
    ("read_only_space", 0x01b7d): (200, "TurboshaftWord32SetTypeMap"),
    ("read_only_space", 0x01ba5): (203, "TurboshaftWord64SetTypeMap"),
    ("read_only_space", 0x01bcd): (197, "TurboshaftFloat64SetTypeMap"),
    ("read_only_space", 0x01bf5): (193, "InternalClassWithSmiElementsMap"),
    ("read_only_space", 0x01c1d): (255, "InternalClassWithStructElementsMap"),
    ("read_only_space", 0x01c45): (182, "OrderedHashMapMap"),
    ("read_only_space", 0x01c6d): (183, "OrderedHashSetMap"),
    ("read_only_space", 0x01c95): (186, "SimpleNumberDictionaryMap"),
    ("read_only_space", 0x01cbd): (180, "NameToIndexHashTableMap"),
    ("read_only_space", 0x01ce5): (247, "EmbedderDataArrayMap"),
    ("read_only_space", 0x01d0d): (177, "EphemeronHashTableMap"),
    ("read_only_space", 0x01d35): (189, "ScriptContextTableMap"),
    ("read_only_space", 0x01d5d): (188, "ObjectBoilerplateDescriptionMap"),
    ("read_only_space", 0x01d85): (246, "CoverageInfoMap"),
    ("read_only_space", 0x01dad): (243, "SideEffectCallHandlerInfoMap"),
    ("read_only_space", 0x01dd5): (243, "SideEffectFreeCallHandlerInfoMap"),
    ("read_only_space", 0x01dfd): (238, "SourceTextModuleMap"),
    ("read_only_space", 0x01e25): (239, "SyntheticModuleMap"),
    ("read_only_space", 0x01e4d): (268, "WasmApiFunctionRefMap"),
    ("read_only_space", 0x01e75): (225, "WasmCapiFunctionDataMap"),
    ("read_only_space", 0x01e9d): (226, "WasmExportedFunctionDataMap"),
    ("read_only_space", 0x01ec5): (270, "WasmInternalFunctionMap"),
    ("read_only_space", 0x01eed): (227, "WasmJSFunctionDataMap"),
    ("read_only_space", 0x01f15): (272, "WasmResumeDataMap"),
    ("read_only_space", 0x01f3d): (274, "WasmTypeInfoMap"),
    ("read_only_space", 0x01f65): (269, "WasmContinuationObjectMap"),
    ("read_only_space", 0x01f8d): (271, "WasmNullMap"),
    ("read_only_space", 0x01fb5): (276, "WeakCellMap"),
    ("read_only_space", 0x20001): (2104, "JSSharedArrayMap"),
    ("read_only_space", 0x20045): (2103, "JSAtomicsMutexMap"),
    ("read_only_space", 0x2006d): (2102, "JSAtomicsConditionMap"),
    ("old_space", 0x02269): (2124, "ExternalMap"),
    ("old_space", 0x02291): (2128, "JSMessageObjectMap"),
}

# List of known V8 objects.
KNOWN_OBJECTS = {
  ("read_only_space", 0x00219): "EmptyFixedArray",
  ("read_only_space", 0x00221): "EmptyWeakFixedArray",
  ("read_only_space", 0x00229): "EmptyWeakArrayList",
  ("read_only_space", 0x00235): "NullValue",
  ("read_only_space", 0x00251): "UndefinedValue",
  ("read_only_space", 0x0026d): "TheHoleValue",
  ("read_only_space", 0x0027d): "EmptyEnumCache",
  ("read_only_space", 0x00289): "EmptyDescriptorArray",
  ("read_only_space", 0x00ab9): "InvalidPrototypeValidityCell",
  ("read_only_space", 0x00db9): "TrueValue",
  ("read_only_space", 0x00dd5): "FalseValue",
  ("read_only_space", 0x00df1): "HashSeed",
  ("read_only_space", 0x00e01): "empty_string",
  ("read_only_space", 0x00ef1): "EmptyPropertyDictionary",
  ("read_only_space", 0x00f1d): "EmptyOrderedPropertyDictionary",
  ("read_only_space", 0x00f41): "EmptySwissPropertyDictionary",
  ("read_only_space", 0x00f61): "EmptyByteArray",
  ("read_only_space", 0x00f69): "EmptyScopeInfo",
  ("read_only_space", 0x00f79): "EmptyPropertyArray",
  ("read_only_space", 0x00f81): "MinusZeroValue",
  ("read_only_space", 0x00f8d): "NanValue",
  ("read_only_space", 0x00f99): "HoleNanValue",
  ("read_only_space", 0x00fa5): "InfinityValue",
  ("read_only_space", 0x00fb1): "MinusInfinityValue",
  ("read_only_space", 0x00fbd): "MaxSafeInteger",
  ("read_only_space", 0x00fc9): "MaxUInt32",
  ("read_only_space", 0x00fd5): "SmiMinValue",
  ("read_only_space", 0x00fe1): "SmiMaxValuePlusOne",
  ("read_only_space", 0x01fdd): "NoOpInterceptorInfo",
  ("read_only_space", 0x02005): "EmptyArrayList",
  ("read_only_space", 0x02011): "EmptyObjectBoilerplateDescription",
  ("read_only_space", 0x0201d): "EmptyArrayBoilerplateDescription",
  ("read_only_space", 0x02029): "EmptyClosureFeedbackCellArray",
  ("read_only_space", 0x02031): "SingleCharacterStringTable",
  ("read_only_space", 0x05af5): "UninitializedValue",
  ("read_only_space", 0x05b2d): "ArgumentsMarker",
  ("read_only_space", 0x05b65): "TerminationException",
  ("read_only_space", 0x05ba5): "Exception",
  ("read_only_space", 0x05bc1): "OptimizedOut",
  ("read_only_space", 0x05bf9): "StaleRegister",
  ("read_only_space", 0x05c31): "SelfReferenceMarker",
  ("read_only_space", 0x05c71): "BasicBlockCountersMarker",
  ("read_only_space", 0x06105): "EmptySlowElementDictionary",
  ("read_only_space", 0x06129): "EmptySymbolTable",
  ("read_only_space", 0x06145): "EmptyOrderedHashMap",
  ("read_only_space", 0x06159): "EmptyOrderedHashSet",
  ("read_only_space", 0x0616d): "EmptyFeedbackMetadata",
  ("read_only_space", 0x06179): "GlobalThisBindingScopeInfo",
  ("read_only_space", 0x06199): "EmptyFunctionScopeInfo",
  ("read_only_space", 0x061bd): "NativeScopeInfo",
  ("read_only_space", 0x061d5): "ShadowRealmScopeInfo",
  ("read_only_space", 0x0fffd): "WasmNull",
  ("old_space", 0x02131): "ArgumentsIteratorAccessor",
  ("old_space", 0x02149): "ArrayLengthAccessor",
  ("old_space", 0x02161): "BoundFunctionLengthAccessor",
  ("old_space", 0x02179): "BoundFunctionNameAccessor",
  ("old_space", 0x02191): "FunctionArgumentsAccessor",
  ("old_space", 0x021a9): "FunctionCallerAccessor",
  ("old_space", 0x021c1): "FunctionNameAccessor",
  ("old_space", 0x021d9): "FunctionLengthAccessor",
  ("old_space", 0x021f1): "FunctionPrototypeAccessor",
  ("old_space", 0x02209): "StringLengthAccessor",
  ("old_space", 0x02221): "ValueUnavailableAccessor",
  ("old_space", 0x02239): "WrappedFunctionLengthAccessor",
  ("old_space", 0x02251): "WrappedFunctionNameAccessor",
  ("old_space", 0x02269): "ExternalMap",
  ("old_space", 0x02291): "JSMessageObjectMap",
  ("old_space", 0x022b9): "EmptyScript",
  ("old_space", 0x02301): "ManyClosuresCell",
  ("old_space", 0x0230d): "ArrayConstructorProtector",
  ("old_space", 0x02321): "NoElementsProtector",
  ("old_space", 0x02335): "MegaDOMProtector",
  ("old_space", 0x02349): "IsConcatSpreadableProtector",
  ("old_space", 0x0235d): "ArraySpeciesProtector",
  ("old_space", 0x02371): "TypedArraySpeciesProtector",
  ("old_space", 0x02385): "PromiseSpeciesProtector",
  ("old_space", 0x02399): "RegExpSpeciesProtector",
  ("old_space", 0x023ad): "StringLengthProtector",
  ("old_space", 0x023c1): "ArrayIteratorProtector",
  ("old_space", 0x023d5): "ArrayBufferDetachingProtector",
  ("old_space", 0x023e9): "PromiseHookProtector",
  ("old_space", 0x023fd): "PromiseResolveProtector",
  ("old_space", 0x02411): "MapIteratorProtector",
  ("old_space", 0x02425): "PromiseThenProtector",
  ("old_space", 0x02439): "SetIteratorProtector",
  ("old_space", 0x0244d): "StringIteratorProtector",
  ("old_space", 0x02461): "NumberStringNotRegexpLikeProtector",
  ("old_space", 0x02475): "StringSplitCache",
  ("old_space", 0x0287d): "RegExpMultipleCache",
  ("old_space", 0x02c85): "BuiltinsConstantsTable",
  ("old_space", 0x031f5): "AsyncFunctionAwaitRejectSharedFun",
  ("old_space", 0x03221): "AsyncFunctionAwaitResolveSharedFun",
  ("old_space", 0x0324d): "AsyncGeneratorAwaitRejectSharedFun",
  ("old_space", 0x03279): "AsyncGeneratorAwaitResolveSharedFun",
  ("old_space", 0x032a5): "AsyncGeneratorYieldWithAwaitResolveSharedFun",
  ("old_space", 0x032d1): "AsyncGeneratorReturnResolveSharedFun",
  ("old_space", 0x032fd): "AsyncGeneratorReturnClosedRejectSharedFun",
  ("old_space", 0x03329): "AsyncGeneratorReturnClosedResolveSharedFun",
  ("old_space", 0x03355): "AsyncIteratorValueUnwrapSharedFun",
  ("old_space", 0x03381): "ErrorStackGetterSharedFun",
  ("old_space", 0x033cd): "ErrorStackSetterSharedFun",
  ("old_space", 0x03419): "PromiseAllResolveElementSharedFun",
  ("old_space", 0x03445): "PromiseAllSettledResolveElementSharedFun",
  ("old_space", 0x03471): "PromiseAllSettledRejectElementSharedFun",
  ("old_space", 0x0349d): "PromiseAnyRejectElementSharedFun",
  ("old_space", 0x034c9): "PromiseCapabilityDefaultRejectSharedFun",
  ("old_space", 0x034f5): "PromiseCapabilityDefaultResolveSharedFun",
  ("old_space", 0x03521): "PromiseCatchFinallySharedFun",
  ("old_space", 0x0354d): "PromiseGetCapabilitiesExecutorSharedFun",
  ("old_space", 0x03579): "PromiseThenFinallySharedFun",
  ("old_space", 0x035a5): "PromiseThrowerFinallySharedFun",
  ("old_space", 0x035d1): "PromiseValueThunkFinallySharedFun",
  ("old_space", 0x035fd): "ProxyRevokeSharedFun",
  ("old_space", 0x03629): "ShadowRealmImportValueFulfilledSFI",
  ("old_space", 0x03655): "SourceTextModuleExecuteAsyncModuleFulfilledSFI",
  ("old_space", 0x03681): "SourceTextModuleExecuteAsyncModuleRejectedSFI",
}

# Lower 32 bits of first page addresses for various heap spaces.
HEAP_FIRST_PAGES = {
  0x00140000: "old_space",
  0x00000000: "read_only_space",
}

# List of known V8 Frame Markers.
FRAME_MARKERS = (
  "ENTRY",
  "CONSTRUCT_ENTRY",
  "EXIT",
  "WASM",
  "WASM_TO_JS",
  "WASM_TO_JS_FUNCTION",
  "JS_TO_WASM",
  "STACK_SWITCH",
  "WASM_DEBUG_BREAK",
  "C_WASM_ENTRY",
  "WASM_EXIT",
  "WASM_LIFTOFF_SETUP",
  "INTERPRETED",
  "BASELINE",
  "MAGLEV",
  "TURBOFAN",
  "STUB",
  "TURBOFAN_STUB_WITH_CONTEXT",
  "BUILTIN_CONTINUATION",
  "JAVA_SCRIPT_BUILTIN_CONTINUATION",
  "JAVA_SCRIPT_BUILTIN_CONTINUATION_WITH_CATCH",
  "INTERNAL",
  "CONSTRUCT",
  "BUILTIN",
  "BUILTIN_EXIT",
  "API_CALLBACK_EXIT",
  "NATIVE",
  "IRREGEXP",
)

# This set of constants is generated from a shipping build.
