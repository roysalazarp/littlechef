
#ifndef MINIFIERS_H
#define MINIFIERS_H

#include "./shared.h"

/** TODO: Make all minifiers work the same, currently the js minifier 
 * minifies inplace and the html minifier minifies into a buffer */

void js_minify(char *content);
size_t html_minify(char *buffer, char *html, size_t html_length);

#endif /* MINIFIERS_H */

