/* libxml2 parser testbed for Olympus E series.
 * cc -o xx xx.c -lxml2
 */
			
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <libxml/parser.h>

void
traverse_tree (int depth, xmlNodePtr node) {
	xmlNodePtr	next;
	xmlChar		*xchar;
	int n;
	char 		*xx;


	if (!node) return;
	xx = malloc(depth * 4 + 1);
	memset (xx, ' ', depth*4);
	xx[depth*4] = 0;

	n = xmlChildElementCount (node);

	next = node;
	do {
		fprintf(stderr,"%snode %s\n", xx,next->name);
		fprintf(stderr,"%selements %d\n", xx,n);
		xchar = xmlNodeGetContent (next);
		fprintf(stderr,"%scontent %s\n", xx,xchar);
		traverse_tree (depth+1,xmlFirstElementChild (next));
	} while ((next = xmlNextElementSibling (next)));
}

void
parse_9301_cmd_tree (xmlNodePtr node) {
	xmlNodePtr next;

	next = xmlFirstElementChild (node);
	do {
		fprintf (stderr,"9301 cmd: %s\n", next->name);
	} while ((next = xmlNextElementSibling (next)));
}

void
parse_value (int type, const char *str) {
	switch (type) {
	case 65535: /* string */
	case 1: /*INT8*/
	case 2: /*UINT8*/
	case 3: /*INT16*/
	case 4: /*UINT16*/
	case 5: /*INT32*/
	case 6: /*UINT32*/
	case 7: /*INT64*/
	case 8: /*UINT64*/
	case 9: /*INT128*/
	case 10: /*UINT128*/
	default:
		fprintf(stderr,"unhandled data type %d!\n", type);
		return;
	}
	return;
}

void
parse_9301_propdesc (xmlNodePtr node) {
	xmlNodePtr next;
	int type = -1;

	fprintf (stderr,"9301 prop: %s\n", node->name);
	next = xmlFirstElementChild (node);
	do {
		if (!strcmp(next->name,"type")) {	/* propdesc.DataType */
			if (!sscanf(xmlNodeGetContent (next), "%04x", &type)) {
				fprintf (stderr,"\ttype %s not parseable\n",xmlNodeGetContent (next));
			}
			else fprintf (stderr,"\ttype is %d\n", type);
			continue;
		}
		if (!strcmp(next->name,"attribute")) {	/* propdesc.GetSet */
			int attr;
			if (!sscanf(xmlNodeGetContent (next), "%02x", &attr)) {
				fprintf (stderr,"\tattr %s not parseable\n",xmlNodeGetContent (next));
			}
			else fprintf (stderr,"\tattribute is %d\n", attr);
			continue;
		}
		if (!strcmp(next->name,"default")) {	/* propdesc.FactoryDefaultValue */
			parse_value (type, xmlNodeGetContent (next));
			continue;
		}
		if (!strcmp(next->name,"value")) {	/* propdesc.CurrentValue */
			parse_value (type, xmlNodeGetContent (next));
			continue;
		}
		fprintf (stderr,"\tpropdescvar: %s\n", next->name);
		traverse_tree (3, next);
	} while ((next = xmlNextElementSibling (next)));
}

void
parse_9301_prop_tree (xmlNodePtr node) {
	xmlNodePtr next;

	next = xmlFirstElementChild (node);
	do {
		parse_9301_propdesc (next);
	} while ((next = xmlNextElementSibling (next)));
}

void
parse_9301_event_tree (xmlNodePtr node) {
	xmlNodePtr next;

	next = xmlFirstElementChild (node);
	do {
		fprintf (stderr,"9301 event: %s\n", next->name);
	} while ((next = xmlNextElementSibling (next)));
}

void
parse_9301_tree (xmlNodePtr node) {
	xmlNodePtr next;

	next = xmlFirstElementChild (node);
	do {
		if (!strcmp (next->name, "cmd")) {
			parse_9301_cmd_tree (next);
			continue;
		}
		if (!strcmp (next->name, "prop")) {
			parse_9301_prop_tree (next);
			continue;
		}
		if (!strcmp (next->name, "event")) {
			parse_9301_event_tree (next);
			continue;
		}
		fprintf (stderr,"9301: unhandled type %s\n", next->name);
	} while ((next = xmlNextElementSibling (next)));
	/*traverse_tree (0, node);*/
}

void
parse_9302_tree (xmlNodePtr node) {
	xmlNodePtr	next;
	xmlChar		*xchar;
	
	next = xmlFirstElementChild (node);
	do {
		if (!strcmp(next->name, "x3cVersion")) {
			int x3cver;
			xchar = xmlNodeGetContent (next);
			sscanf(xchar, "%04x", &x3cver);
			fprintf(stderr,"x3cVersion %d.%d\n", (x3cver>>8)&0xff, x3cver&0xff);
			continue;
		}
		if (!strcmp(next->name, "productIDs")) {
			char *x, *nextspace;
			int len;
			x = xchar = xmlNodeGetContent (next);
			fprintf(stderr,"productIDs:\n", xchar);

			do {
				char *str;

				nextspace=strchr(x,' ');
				if (nextspace) nextspace++;

				/* ascii ptp string, 1 byte lenght, little endian 16 bit chars */
				if (sscanf(x,"%02x", &len)) {
					int i;
					char *str = malloc(len+1);
					for (i=0;i<len;i++) {
						int xc;
						if (sscanf(x+2+i*4,"%04x", &xc)) {
							int cx;

							cx = ((xc>>8) & 0xff) | ((xc & 0xff) << 8);
							str[i] = cx;
						}
						str[len] = 0;
					}
					fprintf(stderr,"\t%s\n", str);
					free (str);
				}
				x = nextspace;
			} while (x);
			continue;
		}
		fprintf (stderr, "unknown node in 9301: %s\n", next->name);
	} while ((next = xmlNextElementSibling (next)));
}

void
traverse_output_tree (xmlNodePtr node) {
	xmlNodePtr next;
	int cmd;

	if (strcmp(node->name,"output")) {
		fprintf(stderr,"node is not output, but %s\n", node->name);
		return;
	}
	if (xmlChildElementCount(node) != 2) {
		fprintf(stderr,"output: expected 2 childs, got %d\n", xmlChildElementCount(node));
		return;
	}
	next = xmlFirstElementChild (node);
	if (!strcmp(next->name,"result")) {
		int result;
		xmlChar *xchar;
		xchar = xmlNodeGetContent (next);
		if (!sscanf(xchar,"%04x",&result))
			fprintf(stderr,"failed scanning result from %s\n", xchar);
		else
			fprintf(stderr,"result is 0x%04x\n", result);
	}
	next = xmlNextElementSibling (next);
	if (!sscanf (next->name, "c%04x", &cmd)) {
		fprintf(stderr,"expected c<HEX>, have %s\n", next->name);
		return;
	}
	fprintf(stderr,"cmd is 0x%04x\n", cmd);
	switch (cmd) {
	case 0x9301:
		return parse_9301_tree (next);
	case 0x9302:
		return parse_9302_tree (next);
	default:
		return traverse_tree (0, next);
	}
}

void
traverse_input_tree (xmlNodePtr node) {
	return traverse_tree (0, node);
}

void
traverse_x3c_tree (xmlNodePtr node) {
	xmlNodePtr	next;
	if (!node) return;
	if (strcmp(node->name,"x3c")) {
		fprintf(stderr,"node is not x3c, but %s\n", node->name);
		return;
	}
	if (xmlChildElementCount(node) != 1) {
		fprintf(stderr,"x3c: expected 1 child, got %d\n", xmlChildElementCount(node));
		return;
	}
	next = xmlFirstElementChild (node);
	if (!strcmp(next->name, "output")) {
		traverse_output_tree (next);
		return;
	}
	if (!strcmp(next->name, "input")) {
		traverse_input_tree (next); /* event */
		return;
	}
	fprintf(stderr,"unknown name %s below x3c\n", next->name);
}

void
parse_xml(char*resp) {
	xmlDocPtr	docin;
	xmlNodePtr	docroot, next;
	xmlChar		*xchar;
	int n;

	docin = xmlReadMemory (resp, strlen(resp), "http://gphoto.org/", "utf-8", 0);
	if (!docin) exit(255);
	docroot = xmlDocGetRootElement (docin);
	if (!docroot) exit(255);
	traverse_x3c_tree (docroot);
}

int
main(int argc, char **argv) {
	parse_xml("<?xml version=\"1.0\"?> <x3c xmlns=\"http://www1.olympus-imaging.com/ww/x3c\"> <output> <result>2001</result> <c9302> <x3cVersion>0100</x3cVersion> <productIDs>224F0045003000360034003000300030003000300030003000300030002D00300030003000300031003000300039002D004700370033003500310039003500360033000000 224F004C003000300032003300300031003000300030003000300030002D00300030003000300031003300300035002D003200310033003000340033003600300037000000</productIDs> </c9302> </output> </x3c>");
	parse_xml("\
<?xml version=\"1.0\"?>\
<x3c xmlns=\"http://www1.olympus-imaging.com/ww/x3c\">\
<output>\
<result>2001</result>\
<c9301>\
<cmd><c1001/>\
<c1002/>\
<c1014/>\
<c1015/>\
<c1016/>\
<c9101/>\
<c9103/>\
<c910C/>\
<c9107/>\
<c910A/>\
<c910B/>\
<c9301/>\
<c9302/>\
<c9501/>\
<c9402/>\
<c9581/>\
<c9482/>\
<c9108/>\
</cmd>\
<event><eC101/>\
<eC102/>\
<eC103/>\
<eC104/>\
</event>\
<prop><p5001>\
<type>0002</type>\
<attribute>00</attribute>\
<default>00</default>\
<value>64</value>\
<range>01 64 01</range>\
</p5001>\
<p5003>\
<type>FFFF</type>\
<attribute>01</attribute>\
<default>0A3400300033003200780033003000320034000000</default>\
<value>0A3400300033003200780033003000320034000000</value>\
<enum>0A3400300033003200780033003000320034000000</enum>\
</p5003>\
<p5007>\
<type>0004</type>\
<attribute>01</attribute>\
<default>017C</default>\
<value>0230</value>\
<enum>017C 0190 01C2 01F4 0230 0276 02C6 0320 0384 03E8 044C 0514 0578 0640 0708 07D0 0898</enum>\
</p5007>\
<p5008>\
<type>0006</type>\
<attribute>00</attribute>\
<default>00002710</default>\
<value>00000D48</value>\
<range>00000AF0 000020D0 00000064</range>\
</p5008>\
<p500A>\
<type>0004</type>\
<attribute>01</attribute>\
<default>0002</default>\
<value>8001</value>\
<enum>0001 0002 8001</enum>\
</p500A>\
<p500B>\
<type>0004</type>\
<attribute>01</attribute>\
<default>8001</default>\
<value>8001</value>\
<enum>0002 0004 8001 8011 8012</enum>\
</p500B>\
<p500C>\
<type>0004</type>\
<attribute>01</attribute>\
<default>0001</default>\
<value>0001</value>\
<enum>0001 0004 8003 8001 8002 0003 0002 9001 9004 9010 9040</enum>\
</p500C>\
<p500E>\
<type>0004</type>\
<attribute>01</attribute>\
<default>0002</default>\
<value>0003</value>\
<enum>FFFF 0001 0002 0003 0004 8001 8002 8003 8006 9006 9008 900A 900F 9011 9013 9014</enum>\
</p500E>\
<p500F>\
<type>0004</type>\
<attribute>01</attribute>\
<default>FFFF</default>\
<value>0190</value>\
<enum>FFFF 0064 007D 00A0 00C8 00FA 0140 0190 01F4 0280 0320 03E8 04E2 0640 07D0 09C4 0C80</enum>\
</p500F>\
<p5010>\
<type>0003</type>\
<attribute>01</attribute>\
<default>0000</default>\
<value>0000</value>\
<enum>EC78 EDC6 EF13 F060 F1AE F2FB F448 F596 F6E3 F830 F97E FACB FC18 FD66 FEB3 0000 014D 029A 03E8 0535 0682 07D0 091D 0A6A 0BB8 0D05 0E52 0FA0 10ED 123A 1388</enum>\
</p5010>\
<p5013>\
<type>0004</type>\
<attribute>01</attribute>\
<default>0001</default>\
<value>0001</value>\
<enum>0001 0022 0012</enum>\
</p5013>\
<p5014>\
<type>0002</type>\
<attribute>01</attribute>\
<default>00</default>\
<value>00</value>\
<enum>FE FF 00 01 02</enum>\
</p5014>\
<p5015>\
<type>0002</type>\
<attribute>01</attribute>\
<default>00</default>\
<value>00</value>\
<enum>FE FF 00 01 02</enum>\
</p5015>\
<p5018>\
<type>0004</type>\
<attribute>01</attribute>\
<default>0001</default>\
<value>0002</value>\
<range>0001 0001 0001</range>\
</p5018>\
<p501C>\
<type>0004</type>\
<attribute>01</attribute>\
<default>8100</default>\
<value>8100</value>\
<enum>8100 8101</enum>\
</p501C>\
<pD102>\
<type>0004</type>\
<attribute>01</attribute>\
<default>0102</default>\
<value>0020</value>\
<enum>0020 0101 0102 0103 0104 0121 0122 0123 0124</enum>\
</pD102>\
<pD103>\
<type>0004</type>\
<attribute>01</attribute>\
<default>0002</default>\
<value>0001</value>\
<enum>0001 0002</enum>\
</pD103>\
<pD104>\
<type>0004</type>\
<attribute>01</attribute>\
<default>0001</default>\
<value>0001</value>\
<enum>0001 0002 0003</enum>\
</pD104>\
<pD105>\
<type>0004</type>\
<attribute>01</attribute>\
<default>0001</default>\
<value>0003</value>\
<enum>0001 0002 0003</enum>\
</pD105>\
<pD106>\
<type>0004</type>\
<attribute>01</attribute>\
<default>014D</default>\
<value>014D</value>\
<enum>03E8 01F4 014D</enum>\
</pD106>\
<pD107>\
<type>0004</type>\
<attribute>01</attribute>\
<default>0001</default>\
<value>0001</value>\
<enum>0001 0002 8000 0004</enum>\
</pD107>\
<pD108>\
<type>0004</type>\
<attribute>01</attribute>\
<default>0001</default>\
<value>0001</value>\
<enum>0001</enum>\
</pD108>\
<pD109>\
<type>0004</type>\
<attribute>01</attribute>\
<default>0BB8</default>\
<value>0BB8</value>\
<enum>0BB8 0FA0 1194 14B4 157C 1770 19C8 1D4C</enum>\
</pD109>\
<pD10A>\
<type>0003</type>\
<attribute>01</attribute>\
<default>0000</default>\
<value>0000</value>\
<enum>FFF9 FFFA FFFB FFFC FFFD FFFE FFFF 0000 0001 0002 0003 0004 0005 0006 0007</enum>\
</pD10A>\
<pD10B>\
<type>0004</type>\
<attribute>01</attribute>\
<default>0001</default>\
<value>0001</value>\
<enum>0001</enum>\
</pD10B>\
<pD10C>\
<type>0004</type>\
<attribute>01</attribute>\
<default>1518</default>\
<value>1518</value>\
<enum>07D0 0802 0834 0866 0898 08CA 08FC 092E 0960 0992 09C4 09F6 0A28 0A5A 0A8C 0ABE 0AF0 0B54 0BB8 0C1C 0C80 0CE4 0D48 0DAC 0E10 0E74 0ED8 0F3C 0FA0 1068 1130 11F8 12C0 1388 1450 1518 15E0 16A8 1770 1838 1900 19C8 1A90 1B58 1CE8 1E78 2008 2198 2328 24B8 2648 2710 2AF8 2EE0 32C8 36B0</enum>\
</pD10C>\
<pD10D>\
<type>0006</type>\
<attribute>01</attribute>\
<default>000100FA</default>\
<value>000A000D</value>\
<enum>0258000A 01F4000A 0190000A 012C000A 00FA000A 00C8000A 0096000A 0082000A 0064000A 0050000A 003C000A 0032000A 0028000A 0020000A 0019000A 0014000A 0010000A 000D000A 000A000A 000A000D 000A0010 000A0014 000A0019 00010003 00010004 00010005 00010006 00010008 0001000A 0001000D 0001000F 00010014 00010019 0001001E 00010028 00010032 0001003C 00010050 00010064 0001007D 000100A0 000100C8 000100FA 00010140 00010190 000101F4 00010280 00010320 000103E8 000104E2 00010640 000107D0 000109C4 00010C80 00010FA0</enum>\
</pD10D>\
<pD10E>\
<type>0006</type>\
<attribute>01</attribute>\
<default>00000000</default>\
<value>00000000</value>\
<enum>00000000 01E00001</enum>\
</pD10E>\
<pD10F>\
<type>0004</type>\
<attribute>01</attribute>\
<default>0002</default>\
<value>0000</value>\
<enum>0000 0001 0002 0003 0004 0005 0007 000A 000F 0014 0019 001E</enum>\
</pD10F>\
<pD11A>\
<type>0003</type>\
<attribute>01</attribute>\
<default>0000</default>\
<value>0000</value>\
<enum>F448 F574 F704 F830 F97E FACB FC18 FD66 FEB3 0000 014D 029A 03E8 0535 0682 07D0 08FC 0A8C 0BB8</enum>\
</pD11A>\
<pD11E>\
<type>0004</type>\
<attribute>01</attribute>\
<default>0001</default>\
<value>0001</value>\
<enum>0000 0001</enum>\
</pD11E>\
<pD11F>\
<type>0004</type>\
<attribute>01</attribute>\
<default>0001</default>\
<value>0001</value>\
<enum>0000 0001</enum>\
</pD11F>\
<pD120>\
<type>0004</type>\
<attribute>01</attribute>\
<default>0001</default>\
<value>0001</value>\
<enum>0000 0001</enum>\
</pD120>\
<pD122>\
<type>0004</type>\
<attribute>01</attribute>\
<default>0001</default>\
<value>0001</value>\
<enum>0001 0002</enum>\
</pD122>\
<pD124>\
<type>0003</type>\
<attribute>01</attribute>\
<default>0000</default>\
<value>0000</value>\
<enum>FFFE FFFF 0000 0001 0002</enum>\
</pD124>\
<pD126>\
<type>0004</type>\
<attribute>01</attribute>\
<default>FFFF</default>\
<value>0000</value>\
<enum>FFFF 0000 0001</enum>\
</pD126>\
<pD127>\
<type>0004</type>\
<attribute>01</attribute>\
<default>0001</default>\
<value>0000</value>\
<enum>0000 0010 0001 0100</enum>\
</pD127>\
<pD129>\
<type>0004</type>\
<attribute>01</attribute>\
<default>0000</default>\
<value>0000</value>\
<enum>0000 0001</enum>\
</pD129>\
<pD12F>\
<type>0006</type>\
<attribute>01</attribute>\
<default>454E5500</default>\
<value>454E5500</value>\
<enum>454E5500 46524100 44455500 45535000 49544100 4A504E00 4B4F5200 43485300 43485400 52555300 43535900 4E4C4400 44414E00 504C4B00 50544700 53564500 4E4F5200 46494E00 48525600 534C5600 48554E00 454C4C00 534B5900 54524B00 4C564900 45544900 4C544800 554B5200 53524200 42475200 524F4D00 494E4400 4D534C00 54484100</enum>\
</pD12F>\
<pD130>\
<type>0004</type>\
<attribute>01</attribute>\
<default>0005</default>\
<value>0005</value>\
<enum>0000 0001 0002 0003 0004 0005 0006 0007 0008 0009 000A 000B 000C 000D 000E 000F 0010 0011 0012 0013 0014 FFFF</enum>\
</pD130>\
<pD131>\
<type>0004</type>\
<attribute>01</attribute>\
<default>003C</default>\
<value>003C</value>\
<enum>0000 003C 00B4 012C 0258</enum>\
</pD131>\
<pD135>\
<type>0004</type>\
<attribute>01</attribute>\
<default>0001</default>\
<value>0002</value>\
<enum>0001 0002</enum>\
</pD135>\
<pD136>\
<type>0004</type>\
<attribute>01</attribute>\
<default>0000</default>\
<value>0000</value>\
<enum>0000 0001</enum>\
</pD136>\
<pD13A>\
<type>0004</type>\
<attribute>01</attribute>\
<default>0000</default>\
<value>0001</value>\
<enum>FC18 0000 0001 03E8</enum>\
</pD13A>\
<pD12B>\
<type>0004</type>\
<attribute>01</attribute>\
<default>014D</default>\
<value>014D</value>\
<enum>03E8 014D</enum>\
</pD12B>\
<pD132>\
<type>0003</type>\
<attribute>01</attribute>\
<default>0000</default>\
<value>0000</value>\
<enum>FFF9 FFFA FFFB FFFC FFFD FFFE FFFF 0000 0001 0002 0003 0004 0005 0006 0007</enum>\
</pD132>\
<pD12C>\
<type>0004</type>\
<default>0000</default>\
<value>0000</value>\
<enum>0000</enum>\
</pD12C>\
<pD12D>\
<type>0004</type>\
<attribute>01</attribute>\
<default>0000</default>\
<value>0000</value>\
<enum>0000</enum>\
</pD12D>\
<pD13B>\
<type>0004</type>\
<attribute>01</attribute>\
<default>0002</default>\
<value>0002</value>\
<enum>0000 0001 0002 8001 8003 8101</enum>\
</pD13B>\
<pD13E>\
<type>0004</type>\
<attribute>01</attribute>\
<default>0002</default>\
<value>0002</value>\
<enum>0002</enum>\
</pD13E>\
<pD13D>\
<type>0004</type>\
<attribute>01</attribute>\
<default>0000</default>\
<value>0000</value>\
<enum>0000 0001 0002</enum>\
</pD13D>\
<pD140>\
<type>0004</type>\
<attribute>01</attribute>\
<default>0008</default>\
<value>0008</value>\
<enum>0008 001E 003C FFFF</enum>\
</pD140>\
<pD141>\
<type>0004</type>\
<attribute>01</attribute>\
<default>0002</default>\
<value>0002</value>\
<enum>0000 0001 0002 8001 8003</enum>\
</pD141>\
<pD143>\
<type>0004</type>\
<attribute>01</attribute>\
<default>0000</default>\
<value>0000</value>\
<enum>0000 0001</enum>\
</pD143>\
<pD144>\
<type>0004</type>\
<attribute>01</attribute>\
<default>0001</default>\
<value>0001</value>\
<enum>0001 0002 0003</enum>\
</pD144>\
<pD145>\
<type>0004</type>\
<attribute>01</attribute>\
<default>0001</default>\
<value>0001</value>\
<enum>0001 0002 0003 0004</enum>\
</pD145>\
<pD146>\
<type>0004</type>\
<attribute>01</attribute>\
<default>0002</default>\
<value>0002</value>\
<enum>0002 0001</enum>\
</pD146>\
<pD147>\
<type>0004</type>\
<attribute>01</attribute>\
<default>0002</default>\
<value>0002</value>\
<enum>0000 0001 0002 0003</enum>\
</pD147>\
<pD148>\
<type>0003</type>\
<attribute>01</attribute>\
<default>0000</default>\
<value>0000</value>\
<enum>EC78 EDC6 EF13 F060 F1AE F2FB F448 F596 F6E3 F830 F97E FACB FC18 FD66 FEB3 0000 014D 029A 03E8 0535 0682 07D0 091D 0A6A 0BB8 0D05 0E52 0FA0 10ED 123A 1388</enum>\
</pD148>\
<pD149>\
<type>0006</type>\
<attribute>01</attribute>\
<default>00010001</default>\
<value>00010001</value>\
<enum>00010001 000A000D 000A0010 00010002 000A0019 00010003 00010004 00010005 00010006 00010008 0001000A 0001000D 00010010 00010014 00010019 00010020 00010028 00010032 00010040 00010050 00010064 00010080</enum>\
</pD149>\
<pD14A>\
<type>0003</type>\
<attribute>01</attribute>\
<default>FFFF</default>\
<value>FFFF</value>\
<enum>FFFF 0000 0001</enum>\
</pD14A>\
<pD14B>\
<type>0004</type>\
<attribute>01</attribute>\
<default>0006</default>\
<value>0006</value>\
<enum>0002 0004 0005 0006 0007 0008 000A</enum>\
</pD14B>\
<pD14E>\
<type>0004</type>\
<attribute>01</attribute>\
<default>0001</default>\
<value>0001</value>\
<enum>0000 0001</enum>\
</pD14E>\
<pD14F>\
<type>0004</type>\
<attribute>01</attribute>\
<default>0003</default>\
<value>0003</value>\
<enum>0001 0002 0003</enum>\
</pD14F>\
<pD150>\
<type>0004</type>\
<attribute>01</attribute>\
<default>00C8</default>\
<value>00C8</value>\
<enum>00C8 00FA 0140 0190 01F4 0280 0320 03E8 04E2 0640 07D0 09C4 0C80</enum>\
</pD150>\
<pD151>\
<type>0004</type>\
<attribute>01</attribute>\
<default>0320</default>\
<value>0320</value>\
<enum>00C8 00FA 0140 0190 01F4 0280 0320 03E8 04E2 0640 07D0 09C4 0C80</enum>\
</pD151>\
<pD152>\
<type>0004</type>\
<attribute>01</attribute>\
<default>0008</default>\
<value>0008</value>\
<enum>0001 0002 0004 0008 000F 0014 0019 001E</enum>\
</pD152>\
<pD153>\
<type>0004</type>\
<attribute>01</attribute>\
<default>0001</default>\
<value>0001</value>\
<enum>0001 0010</enum>\
</pD153>\
<pD154>\
<type>0004</type>\
<attribute>01</attribute>\
<default>015E</default>\
<value>015E</value>\
<range>0001 270F 0001</range>\
</pD154>\
<pD157>\
<type>0004</type>\
<attribute>01</attribute>\
<default>0000</default>\
<value>0000</value>\
<enum>0000 0001</enum>\
</pD157>\
<pD158>\
<type>0004</type>\
<attribute>01</attribute>\
<default>0001</default>\
<value>0001</value>\
<enum>0000 0001</enum>\
</pD158>\
<pD155>\
<type>0004</type>\
<attribute>01</attribute>\
<default>0000</default>\
<value>0000</value>\
<enum>0011 0012 0013 0014 0021 0022 0023 0024 0031 0032 0033 0034 0000</enum>\
</pD155>\
<pD159>\
<type>0004</type>\
<attribute>01</attribute>\
<default>0000</default>\
<value>0000</value>\
<enum>0000 0303 0307 030A</enum>\
</pD159>\
<pD15A>\
<type>0004</type>\
<attribute>01</attribute>\
<default>0000</default>\
<value>0000</value>\
<enum>0000 0314 0328 033C</enum>\
</pD15A>\
<pD15B>\
<type>0004</type>\
<attribute>01</attribute>\
<default>0000</default>\
<value>0000</value>\
<enum>0000 0314 0328 033C</enum>\
</pD15B>\
<pD15C>\
<type>0004</type>\
<attribute>01</attribute>\
<default>0000</default>\
<value>0000</value>\
<enum>0000 0303 0307 030A</enum>\
</pD15C>\
<pD15D>\
<type>0004</type>\
<attribute>01</attribute>\
<default>0000</default>\
<value>0000</value>\
<enum>0000 0303 0307 030A</enum>\
</pD15D>\
<pD15F>\
<type>0004</type>\
<attribute>01</attribute>\
<default>0000</default>\
<value>0000</value>\
<enum>0000 0001</enum>\
</pD15F>\
<pD160>\
<type>0004</type>\
<attribute>01</attribute>\
<default>0001</default>\
<value>0001</value>\
<enum>0001 0002</enum>\
</pD160>\
<pD161>\
<type>0006</type>\
<attribute>01</attribute>\
<default>00040003</default>\
<value>00040003</value>\
<enum>00040003 00030002 00100009 00060006</enum>\
</pD161>\
</prop>\
</c9301>\
</output>\
</x3c>");
	return 0;

}
