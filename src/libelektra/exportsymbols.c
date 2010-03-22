#include <stdio.h>

int main(int argc, char**argv)
{
	char toolsfunc[][20] = {"ksFromXMLfile", "ksFromXML", "ksToStream",
		"ksOutput", "ksGenerate", "keyToStream", "keyOutput",
		"keyGenerate"};
	int i,j;

	/*Exit if no backend is given with error code, because
	  argv[1] is used below*/
	if (argc < 2) return 1;

	FILE *f = fopen("exported_symbols.h", "w");
	fprintf (f, "/* exported_symbols.h generated by exportsymbols */\n\n"

			"#include <kdbloader.h>\n\n"

			"/* The static case\n"
			" *\n"
			" * Struct which contain export symbols\n"
			" *  Format :\n"
			" *  --------\n"
			" *\n"
			" *  filename, NULL\n"
			" *  symbol1, &func1,\n"
			" *  symbol2, &func2,\n"
			" *  filename2, NULL\n"
			" *  symbol1, &func1,\n"
			" *  symbol2, &func2,\n"
			" *  ....\n"
			" *  symboln, &funcn,\n"
			" *  NULL, NULL\n"
			" */\n\n"

			"typedef struct {\n"
			"	const char *name;\n"
			"	void (*function)(void);\n"
			"} kdblib_symbol;\n\n"

			"extern kdblib_symbol kdb_exported_syms[];\n\n"

			);

	for (i=1; i<argc; ++i)
	{
		if (!strcmp (argv[i], "libelektratools"))
		{
			for (j=0; j<sizeof(toolsfunc)/20; ++j)
			{
				fprintf(f, "extern void libelektratools_LTX_%s (void);\n", toolsfunc[j]);
			}
		} else {
			fprintf(f, "extern void libelektra_%s_LTX_kdbBackendFactory (void);\n", argv[i]);
		}
	}

	fclose (f);

	f = fopen("exported_symbols.c", "w");

	fprintf(f, "/* exported_symbols.c generated by exportsymbols.sh */\n\n"

			"#include <exported_symbols.h>\n\n"

			"kdblib_symbol kdb_exported_syms[] =\n"
			"{\n");

	printf ("Exporting symbols for default...\n");
	fprintf(f, "\t{\"libelektra-default\", 0},\n");
	fprintf(f, "\t{\"kdbBackendFactory\", &libelektra_%s_LTX_kdbBackendFactory},\n", argv[1]);

	for (i=1; i<argc; ++i)
	{
		if (!strcmp (argv[i], "libelektratools"))
		{
			fprintf(f, "\t{\"libelektratools\", 0},\n");
			for (j=0; j<sizeof(toolsfunc)/20; ++j)
			{
				fprintf(f, "\t{\"%s\", &libelektratools_LTX_%s},\n", toolsfunc[j], toolsfunc[j]);
			}
		} else {
			printf ("Exporting symbols for %s ...\n", argv[i]);
			fprintf(f, "\t{\"libelektra-%s\", 0},\n", argv[i]);
			fprintf(f, "\t{\"kdbBackendFactory\", &libelektra_%s_LTX_kdbBackendFactory},\n", argv[i]);
		}
	}

	fprintf(f, "\t{ 0 , 0 }\n");
	fprintf(f, "};\n");

	fclose (f);
	return 0;
}
