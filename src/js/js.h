#ifndef JS_H__
#define JS_H__


#include "showtime.h"
#include "prop/prop.h"
#include "ext/spidermonkey/jsapi.h"

LIST_HEAD(js_route_list, js_route);
LIST_HEAD(js_searcher_list, js_searcher);
LIST_HEAD(js_plugin_list, js_plugin);

/**
 *
 */
typedef struct js_plugin {
  LIST_ENTRY(js_plugin) jsp_link;

  char *jsp_url;

  struct js_route_list jsp_routes;
  struct js_searcher_list jsp_searchers;

} js_plugin_t;


JSContext *js_newctx(void);

JSBool js_httpRequest(JSContext *cx, JSObject *obj, uintN argc,
		      jsval *argv, jsval *rval);

JSBool js_readFile(JSContext *cx, JSObject *obj, uintN argc,
		   jsval *argv, jsval *rval);

JSBool js_addURI(JSContext *cx, JSObject *obj, uintN argc, 
		 jsval *argv, jsval *rval);

JSBool js_addSearcher(JSContext *cx, JSObject *obj, uintN argc, 
		      jsval *argv, jsval *rval);

struct navigator;
struct backend;

struct nav_page *js_backend_open(struct backend *be, struct navigator *nav, 
				 const char *url, const char *view,
				 char *errbuf, size_t errlen);

void js_backend_search(struct backend *be, struct prop *model, 
		       const char *query);

int js_plugin_load(const char *url, char *errbuf, size_t errlen);

int js_prop_from_object(JSContext *cx, JSObject *obj, prop_t *p);

void js_prop_set_from_jsval(JSContext *cx, prop_t *p, jsval value);

void js_page_flush_from_plugin(JSContext *cx, js_plugin_t *jp);

#endif // JS_H__ 