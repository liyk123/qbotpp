cmake_minimum_required (VERSION 3.12)

message("CMAKE_VERSION: ${CMAKE_VERSION}") 
message("CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")

if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

set(PROJECT_NAME "qbotpp")

project (${PROJECT_NAME})

set(FILE_LIST
    "main.cpp"
)

find_package(Drogon CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)


macro(get_git_hash _git_hash)
  find_package(Git QUIET)
  if(GIT_FOUND)
    execute_process(
      COMMAND ${GIT_EXECUTABLE} log -1 --pretty=format:%H
      OUTPUT_VARIABLE ${_git_hash}
      OUTPUT_STRIP_TRAILING_WHITESPACE
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
  endif()
endmacro()

macro(get_git_branch _git_branch)
  find_package(Git QUIET)
  if(GIT_FOUND)
    execute_process(
      COMMAND ${GIT_EXECUTABLE} symbolic-ref --short -q HEAD
      OUTPUT_VARIABLE ${_git_branch}
      OUTPUT_STRIP_TRAILING_WHITESPACE
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
  endif()
endmacro()

macro(get_git_date_time _git_date_time)
  find_package(Git QUIET)
  if(GIT_FOUND)
    execute_process(
      COMMAND ${GIT_EXECUTABLE} log -1 --format=%cd --date=short
      OUTPUT_VARIABLE ${_git_date_time}
      OUTPUT_STRIP_TRAILING_WHITESPACE
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
  endif()
endmacro()

set(GIT_HASH "unknown")
get_git_hash(GIT_HASH)
add_definitions(-DGIT_VERSION="${GIT_HASH}")

set(GIT_BRANCH "unknown") 
get_git_branch(GIT_BRANCH)
add_definitions(-DGIT_BRANCH="${GIT_BRANCH}")

set(GIT_DATE "unknown") 
get_git_date_time(GIT_DATE)
add_definitions(-DGIT_DATE="${GIT_DATE}")

add_definitions(-DPROJECT_NAME="${PROJECT_NAME}")

add_executable(${PROJECT_NAME} ${FILE_LIST})

target_link_libraries(${PROJECT_NAME} PRIVATE 
    PRIVATE Drogon::Drogon
    PRIVATE nlohmann_json::nlohmann_json
)

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)

target_compile_definitions(${PROJECT_NAME} PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
