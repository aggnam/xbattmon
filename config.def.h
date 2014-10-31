struct {
	char *name;
	unsigned long pixel;
} colmap[] = {
	[COLOR_BATT_CHARGED]     = { "green", 0 },
	[COLOR_BATT_LEFT2CHARGE] = { "grey",  0 },
	[COLOR_BATT_DRAINED]     = { "red",   0 },
	[COLOR_BATT_LEFT2DRAIN]  = { "blue",  0 }
};

unsigned int thickness = 4;	/* 4 pixels by default */
time_t pollinterval = 5;	/* poll battery state every 5 seconds */
int bottom = 1;			/* set to 0 if you want the bar to be at the top */
int maxcap = 100;		/* maximum battery capacity */
