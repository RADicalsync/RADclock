// Borrowed from a Google kernel project
// URL: https://kernel.googlesource.com/pub/scm/linux/kernel/git/luto/misc-tests/+/5655bd41ffedc002af69e3a8d1b0a168c22f2549/dump-vdso.c

#include <stdio.h>
#include <string.h>
#include <unistd.h>
int main()
{
        FILE *maps;
        void *vdso_begin, *vdso_end;
        int found_vdso = 0;
        maps = fopen("/proc/self/maps", "r");
        char buf[1024];
        while (fgets(buf, 1024, maps)) {
                if (strstr(buf, "[vdso]")) {
                        found_vdso = 1;
                        break;
                }
        }
        fclose(maps);
        if (!found_vdso) {
                fprintf(stderr, "Could not find vdso mapping\n");
                return 1;
        }
        sscanf(buf, "%p-%p", &vdso_begin, &vdso_end);
        write(1, vdso_begin, vdso_end - vdso_begin);
        return 0;
}
