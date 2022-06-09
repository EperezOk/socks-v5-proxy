#include "../include/monitor.h"

extern void
monitor_parser_init(struct monitor_parser *p) {
    p->state = monitor_version;
    memset(p->request, 0, sizeof(*(p->request)));
}

static enum monitor_state
version(const uint8_t c, struct monitor_parser *p) {
    enum monitor_state next;
    switch (c) {
        case 0x01:
            next = monitor_method;
            break;
        default:
            next = monitor_error_unsupported_version;
            break;
    }

    return next;
}

static enum monitor_state
method(const uint8_t c, struct monitor_parser *p) {
    enum monitor_state next;
    switch (c) {
        case method_get:
        case method_config:
            next = monitor_target;
            break;
        default:
            next = monitor_error_unsupported_method;
            break;
    }
    p->request->method = c;
    return next;
}

static bool
is_valid_pop3_disector(struct monitor_parser *p) {
	// TODO
}

static enum monitor_state
target(const uint8_t c, struct monitor_parser *p) {
    enum monitor_state next;
    switch(p->request->method) {
        case method_get:
            switch (c) {
                case target_get_historic:
                case target_get_concurrent:
                case target_get_transfered:
                    next = monitor_dlen;
					memset(&(p->request->target), 0, sizeof(int));
					p->request->target.get = c;
                    break;
                default:
                    next = monitor_error_unsupported_target;
                    break;
            }
            break;
        case method_config:
            switch (c) {
                case target_config_pop3disector:
                    next = monitor_dlen;
					memset(&(p->request->target), 0, sizeof(struct monitor_req_target_config));
					struct monitor_req_target_config config = {
						.option	= c,
						.isValid = is_valid_pop3_disector,
					};
					memcpy(p->request->target.config, config, sizeof(struct monitor_req_target_config));
                    break;
                default:
                    next = monitor_error_unsupported_target;
                    break;
            }
            break;
        default:
            // impossible
            break;
    }

    return next;
}

static enum monitor_state
dlen(const uint8_t c, struct monitor_parser *p) {
    p->request->len = c;
    p->i = 0;
    p->len = c;
    return monitor_data;
}

static enum monitor_state
data(const uint8_t c, struct monitor_parser *p) {
    p->request->data[p->i++] = c;
    if (p->i == p->len) {
        if (p->request->target->config.isValid(p)) 
		// validar data
        return monitor_ulen;
		return monitor_error_unsupported_data
    }
    return monitor_data;
}

// TODO: mover toda la logica al parser de auth, y delegar aca a ese parser
static enum monitor_state
ulen(const uint8_t c, struct monitor_parser *p) {
    p->i = 0;
    p->len = c;
    return monitor_uname;
}

static enum monitor_state
uname(const uint8_t c, struct monitor_parser *p) {
    p->uname[p->i++] = c;
    if (p->i == p->len)
        return monitor_plen;
    return monitor_uname;
}

static enum monitor_state
plen(const uint8_t c, struct monitor_parser *p) {
    p->i = 0;
    p->len = c;
    return monitor_passwd;
}

static enum monitor_state
passwd(const uint8_t c, struct monitor_parser *p) {
    p->passwd[p->i++] = c;
    if (p->i == p->len)
        return monitor_authenticate;
    return monitor_passwd;
}

static enum monitor_state
authenticate(const uint8_t c, struct monitor_parser *p) {
	// TODO
}

/** entrega un byte al parser, retorna true si se llego al final */
static enum monitor_state 
monitor_parser_feed(struct monitor_parser *p, const uint8_t c) {
    enum monitor_state next;

    switch(p->state) {
        case monitor_version:
            next = version(c, p);
            break;
        case monitor_method:
            next = method(c, p);
            break;
        case monitor_target:
            next = target(c, p);
            break;
        case monitor_dlen:
            next = dlen(c, p);
            break;
        case monitor_data:
            next = data(c, p);
            break;
        case monitor_ulen:
            next = ulen(c, p);
            break;
        case monitor_uname:
            next = uname(c, p);
            break;
        case monitor_plen:
            next = plen(c, p);
            break;
        case monitor_passwd:
            next = passwd(c, p);
            break;
		case monitor_authenticate:
            next = authenticate(c, p);
            break;
        case monitor_done:
        case monitor_error:
        case monitor_error_unsupported_version:
        case monitor_error_unsupported_method:
        case monitor_error_unsupported_target:
        case monitor_error_unsupported_data:
        case monitor_error_unsupported_auth:
            next = p->state;
            break;
        default:
            next = monitor_error;
            break;
    }

    return p->state = next;
}