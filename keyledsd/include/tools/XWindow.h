/* Keyleds -- Gaming keyboard tool
 * Copyright (C) 2017 Julien Hartmann, juli1.hartmann@gmail.com
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
/** @file
 * @brief C++ wrapper for Xlib events
 *
 * This simple wrapper presents a C++ interface for reading and watching a
 * a limited set of information about windows and devices from an X display.
 */
#ifndef TOOLS_WINDOW_H_F1434518
#define TOOLS_WINDOW_H_F1434518

#include <X11/Xlib.h>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace std {
    template <> struct default_delete<::Display> { void operator()(::Display *p) const; };
}

namespace xlib {

class Display;

/****************************************************************************/

/** X window wrapper
 *
 * Simple wrapper to query information about an X display window.
 */
class Window final
{
public:
    typedef ::Window handle_type;
public:
                            Window(Display & display, handle_type window);

    Display &               display() const { return m_display; }
    handle_type             handle() const { return m_window; }

    void                    changeAttributes(unsigned long mask, const XSetWindowAttributes & attrs);

    std::string             name() const;
    std::string             iconName() const;
    const std::string &     className() const
                                { if (!m_classLoaded) { loadClass(); } return m_className; }
    const std::string &     instanceName() const
                                { if (!m_classLoaded) { loadClass(); } return m_instanceName; }

    std::string             getProperty(Atom atom, Atom type) const;

private:
    void                    loadClass() const;

private:
    Display &               m_display;          ///< Display the window belongs to
    handle_type             m_window;           ///< Window handle
    mutable std::string     m_className;        ///< Cached window class name
    mutable std::string     m_instanceName;     ///< Cached window instance name
    mutable bool            m_classLoaded;      ///< Set when m_className and m_instanceName are loaded
};

/****************************************************************************/

/* XInput device wrapper
 *
 * Simple wrapper to watch events generated by a XInput device.
 */
class Device final
{
public:
    typedef int             handle_type;
    static constexpr handle_type invalid_device = 0;
public:
                            Device(Display & display, handle_type device);
                            Device(const Device &) = delete;
                            Device(Device &&);
                            ~Device();

    Display &               display() const { return m_display; }
    handle_type             handle() const { return m_device; }
    const std::string &     devNode() const { return m_devNode; }

    void                    setEventMask(const std::vector<int> & events);

    std::string             getProperty(Atom atom, Atom type) const;

private:
    Display &               m_display;          ///< Display the device belongs to
    handle_type             m_device;           ///< Device handle
    std::string             m_devNode;          ///< Path to device node
};

/****************************************************************************/

/* X display wrapper
 *
 * Manages a connection to an X window system through Xlib.
 * Note that all Window instances returned by the display must be destroyed
 * before destroying the display.
 */
class Display final
{
public:
    typedef ::Display *     handle_type;
    typedef std::map<std::string, Atom> atom_map;
    typedef int             event_type;
    typedef void            (*event_handler)(const XEvent &, void * data);
private:
    struct HandlerInfo {
        event_type      event;
        event_handler   handler;
        void *          data;
    };
public:
                            Display(std::string name = std::string());
                            Display(const Display &) = delete;
    Display &               operator=(const Display &) = delete;
                            ~Display();

    // Properties
    const std::string &     name() const { return m_name; }
    handle_type             handle() const { return m_display.get(); }
    Window &                root() { return m_root; }
    const Window &          root() const { return m_root; }
    Atom                    atom(const std::string & name) const;

    // Event handling
    int                     connection() const;
    void                    processEvents();
    void                    registerHandler(event_type, event_handler, void * data = nullptr);
    void                    unregisterHandler(event_handler);

    std::unique_ptr<Window> getActiveWindow();

private:
    static std::unique_ptr<::Display> openDisplay(const std::string &);

private:
    std::unique_ptr<::Display> m_display;
    std::string             m_name;
    Window                  m_root;
    mutable atom_map        m_atomCache;
    std::vector<HandlerInfo> m_handlers;        ///< Callback list
};

/****************************************************************************/

class Error : public std::runtime_error
{
public:
                            Error(const std::string & msg)
                             : std::runtime_error(msg) {}
                            Error(::Display *display, XErrorEvent *event)
                             : std::runtime_error(makeMessage(display, event)) {}
private:
    static std::string      makeMessage(::Display *display, XErrorEvent *event);
};

/****************************************************************************/

class ErrorCatcher
{
public:
    typedef std::vector<Error> error_list;
    typedef int (*handler_type)(::Display *, XErrorEvent *);
public:
    ErrorCatcher();
    ErrorCatcher(const ErrorCatcher &) = delete;
    ~ErrorCatcher();

    const error_list & errors() const { return m_errors; }
    operator bool() const { return !m_errors.empty(); }

    void synchronize(Display &) const;
private:
    static int errorHandler(::Display *, XErrorEvent *);

private:
    static ErrorCatcher * s_current;
    error_list      m_errors;
    handler_type    m_oldHandler;
    ErrorCatcher *  m_oldCatcher;
};

}

#endif
