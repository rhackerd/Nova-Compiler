project('nova', 'c' , 'cpp',
    version : '0.1.0',
    default_options : [
        'cpp_std=c++23',
        'warning_level=3',
    ]
)

# --- prefixes ---
compiler_src  = 'compiler/src/'
core_src      = 'core/src/'
builder_src   = 'builder/src/'

compiler_inc  = include_directories('compiler/inc')
core_inc      = include_directories('core/inc')
builder_inc   = include_directories('builder/inc')

# --- libraries
llvm_dep = dependency('llvm', version : '>=15.0', modules : ['core', 'AArch64'])
# --- core (static lib) ---
core_sources = files(
    core_src + 'lexer.cpp',
    core_src + 'parser.cpp',
    core_src + 'codegen.cpp',
    core_src + 'module.cpp'
)

nova_core = static_library('nova_core',
    core_sources,
    include_directories : core_inc,
    dependencies: llvm_dep,
)

# --- compiler (exec) ---
compiler_sources = files(
    compiler_src + 'main.cpp',
)

executable('novac',
    compiler_sources,
    include_directories : [compiler_inc, core_inc],
    link_with           : nova_core,
)

# --- builder (exec) ---
builder_sources = files(
    builder_src + 'main.cpp',
    builder_src + 'config.cpp',
)

executable('novab',
    builder_sources,
    include_directories : [builder_inc, core_inc],
    link_with           : nova_core,
)