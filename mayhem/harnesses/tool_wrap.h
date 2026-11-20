// Shared glue for running an ncnn converter tool's main() in-process under libFuzzer:
// the tool source is #included with main renamed to tool_main, and fopen/fclose are tracked
// so FILE*s the tool leaks on its error paths are closed after every input.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <set>
#include <string>
#include <vector>
#include <map>
#include <algorithm>

static std::set<FILE*>& tw_open_files()
{
    static std::set<FILE*> s;
    return s;
}
static FILE* tw_fopen(const char* path, const char* mode)
{
    FILE* fp = fopen(path, mode);
    if (fp) tw_open_files().insert(fp);
    return fp;
}
static int tw_fclose(FILE* fp)
{
    tw_open_files().erase(fp);
    return fclose(fp);
}
static void tw_close_leaked()
{
    std::set<FILE*>& s = tw_open_files();
    for (std::set<FILE*>::iterator it = s.begin(); it != s.end(); ++it)
        fclose(*it);
    s.clear();
}
// Private scratch dir under $TMPDIR (fallback /tmp), created once per process. Every file
// handed out by tw_scratch() is unlinked after each input (tw_cleanup) and, with the dir,
// again at exit, so no input's files outlive it and no process leaves its dir behind.
static std::string& tw_dir()
{
    static std::string dir;
    return dir;
}
static std::set<std::string>& tw_files()
{
    static std::set<std::string> s;
    return s;
}
static void tw_cleanup()
{
    std::set<std::string>& s = tw_files();
    for (std::set<std::string>::iterator it = s.begin(); it != s.end(); ++it)
        unlink(it->c_str());
}
static void tw_remove_dir()
{
    tw_cleanup();
    if (!tw_dir().empty()) rmdir(tw_dir().c_str());
}
static std::string tw_scratch(const char* name)
{
    std::string& dir = tw_dir();
    if (dir.empty())
    {
        const char* base = getenv("TMPDIR");
        if (!base || !*base) base = "/tmp";
        std::string tmpl = std::string(base) + "/ncnn-tool-fuzz-XXXXXX";
        std::vector<char> buf(tmpl.begin(), tmpl.end());
        buf.push_back('\0');
        if (!mkdtemp(&buf[0]))
        {
            fprintf(stderr, "tool_wrap: mkdtemp(%s) failed\n", tmpl.c_str());
            abort();
        }
        dir = &buf[0];
        atexit(tw_remove_dir);
    }
    std::string path = dir + "/" + name;
    tw_files().insert(path);
    return path;
}
static void tw_write(const std::string& path, const uint8_t* data, size_t size)
{
    FILE* fp = fopen(path.c_str(), "wb");
    if (!fp) abort();
    if (size) fwrite(data, 1, size, fp);
    fclose(fp);
}

#define main tool_main
#define fopen tw_fopen
#define fclose tw_fclose
