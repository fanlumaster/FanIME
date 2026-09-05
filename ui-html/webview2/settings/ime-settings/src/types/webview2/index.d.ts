//
// Comes from: https://github.com/MicrosoftEdge/WebView2Feedback/discussions/4028
//

// Type definitions for WebView2Script
// Project: https://learn.microsoft.com/en-us/microsoft-edge/webview2/reference/javascript/
// Definitions by jeremyong <https://github.com/jeremyong>
// Definitions: https://github.com/DefinitelyTyped/DefinitelyTyped

// These declarations correspond to methods and objects used to facilitate communication
// between the host and browser for the Microsoft Edge WebView2 applications.


/**
 * An asynchronous host object proxy. Host objects added via CoreWebView2.AddHostObjectToScript
 * are exposed as host object proxies using window.chrome.webview.hostObjects.{name}. Host object
 * proxies are promises, and resolve to an object representing the host object. The promise is
 * rejected if the app has not added an object with the name. When JavaScript code access a
 * property or method of the object, a promise is returned. The promise resolves to the value
 * that's returned from the host for the property or method. The promise is rejected in case of
 * error; for example, no property or method on the object, or parameters are not valid.
 * 
 * Host object proxies are JavaScript Proxy objects that intercept all property get, property set,
 * and method invocations. Properties or methods that are a part of the Function or Object
 * prototype are run in the JavaScript engine of the current document. Additionally any property
 * or method in the chrome.webview.hostObjects.options.forceLocalProperties array are also run
 * in the JavaScript engine of the current document. This defaults to including optional methods
 * that have meaning in JavaScript like toJSON and Symbol.toPrimitive. Add more to the array as
 * required.
 */
export interface HostObjectAsyncProxyBase {
  [key: string]: Promise<any> | any;

  /**
   * Perform a method invocation on the host object that corresponds to this proxy.
   * All parameters are converted to call the host object method.
   * @param argArray An array of arguments to pass to the host object method invocation.
   * @returns A promise representing the converted value of the return value of the
   *          host object method invocation.
   */
  applyHostFunction(argArray?: any): Promise<any>;

  /**
   * Perform a property get on the host object. Use this method to explicitly force a
   * property get to occur remotely if a conflicting local method or property exists.
   * For instance, proxy.toString() runs the local toString method on the proxy object.
   * But proxy.applyHostFunction('toString') runs toString on the host proxied object
   * instead.
   * @param propertyName String name of the property of which to get the value.
   * @returns A promise representing the converted value of the property of the host
   *          object's property.
   */
  getHostProperty(propertyName: string): Promise<any>;

  /**
   * Perform a property get locally on the proxy object. Use the methods to force getting a
   * property on the host object proxy rather than on the host object it represents. For
   * instance, proxy.unknownProperty gets the property named unknownProperty from the host
   * proxied object. But proxy.getLocalProperty('unknownProperty') gets the value of the
   * property unknownProperty on the proxy object.
   * @param propertyName Name of the property to get the value of.
   * @returns A promise representing the value of the property.
   */
  getLocalProperty(propertyName: string): any;

  /**
   * Perform a property set on the host object. Use this method to explicitly force a property
   * set to occur remotely if a conflicting local method or property exists.
   * @param propertyName Name of the property of which to set the value.
   * @param propertyValue Value to set the property.
   * @returns A promise representing the converted value of the property of the host object's
   *          property. This promise only resolves after the property value has been changed.
   */
  setHostProperty(propertyName: string, propertyValue: any): Promise<any>;

  /**
   * Perform a property set locally on the proxy object. Use the methods to force setting a
   * property on the host object proxy rather than on the host object it represents. For
   * instance, proxy.unknownProperty = 2 sets the property named unknownProperty on the host
   * proxied object. But proxy.setLocalProperty('unknownProperty', 2) sets the value of the
   * property unknownProperty on the proxy object.
   * @param propertyName Name of the property to get the value of.
   * @param propertyValue Value to set the property to.
   * @returns A promise representing the value of the property after it is set. This promise
   *          only resolves after the property changes value.
   */
  setLocalProperty(propertyName: string, propertyValue: any): any;
}


/**
 * An asynchronous host object proxy. Host objects added via CoreWebView2.AddHostObjectToScript
 * are exposed as host object proxies using window.chrome.webview.hostObjects.{name}. Host object
 * proxies are promises, and resolve to an object representing the host object. The promise is
 * rejected if the app has not added an object with the name. When JavaScript code access a
 * property or method of the object, a promise is returned. The promise resolves to the value
 * that's returned from the host for the property or method. The promise is rejected in case of
 * error; for example, no property or method on the object or parameters are not valid.
 * 
 * Host object proxies are JavaScript Proxy objects that intercept all property get, property set,
 * and method invocations. Properties or methods that are a part of the Function or Object
 * prototype are run in the JavaScript engine of the current document. Additionally any property
 * or method in the chrome.webview.hostObjects.options.forceLocalProperties array are also run in
 * the JavaScript engine of the current document. This defaults to including optional methods that
 * have meaning in JavaScript like toJSON and Symbol.toPrimitive. Add more to the array as required.
 */
export interface HostObjectAsyncProxy extends HostObjectAsyncProxyBase {
  [key: string]: Promise<any> | any;

  /**
   * A method which returns a promise for a synchronous host object proxy for the same host
   * object. For example, chrome.webview.hostObjects.sample.methodCall() returns an asynchronous
   * host object proxy. Use the sync method to obtain a synchronous host object proxy instead:
   * const syncProxy = await chrome.webview.hostObjects.sample.methodCall().sync().
   * @returns A promise representing the synchronous host object proxy.
   */
  sync(): Promise<HostObjectSyncProxy>;
}

/**
 * Contains asynchronous proxies for all host objects added via CoreWebView2.AddHostObjectToScript
 * as well as options to configure those proxies, and the container for synchronous proxies.
 * 
 * If you call coreWebView2.AddHostObjectToScript("myObject", object); in your native code, an
 * asynchronous proxy for object is available to your web-side code, by using
 * chrome.webview.hostObjects.myObject.
 */
export type HostObjectsAsyncRoot =
  {
    /**
     * Contains options applicable to CoreWebView2.AddHostObjectToScript added script proxies.
     */
    options: HostObjectsOptions;

    /**
     * Contains synchronous proxies for all host objects added via CoreWebView2.AddHostObjectToScript.
     * 
     * If you call coreWebView2.AddHostObjectToScript("myObject", object); in your native code,
     * a synchronous proxy for object is available to your web-side code, by using
     * chrome.webview.hostObjects.sync.myObject.
     */
    sync: HostObjectsSyncRoot;

    /**
     * Performs a best effort garbage collection on host object proxies that are no longer in use.
     */
    cleanupSome(): void;
  } & {
    [key: string]: HostObjectAsyncProxy;
  }

/**
 * Contains options applicable to CoreWebView2.AddHostObjectToScript added script proxies.
 */
export interface HostObjectsOptions {
  /**
   * When calling a method on a synchronous proxy, the result should also be a synchronous
   * proxy. But in some cases, the sync or async context is lost (for example, when providing
   * to native code a reference to a function, and then calling that function in native code).
   * In these cases, the proxy will be asynchronous if this option is false, and synchronous
   * if this option is true.
   */
  defaultSyncProxy: boolean;

  /**
   * This is an array of regular expressions. When calling a method on a synchronous proxy,
   * the method call will be performed asynchronously if the method name matches a string
   * or regular expression in this array. Setting this value to /Async$/ will make any method
   * that ends with Async be an asynchronous method call. If an async method doesn't match
   * here and isn't forced to be asynchronous, the method will be invoked synchronously,
   * blocking execution of the calling JavaScript and then returning the resolution of the
   * promise, rather than returning a promise.
   * 
   * This defaults to [].
   */
  forceAsyncMethodMatches: RegExp[];

  /**
   * This is an array of host object property names that will be run locally, instead of being
   * called on the native host object. This defaults to ['then', 'toJSON', Symbol.toString,
   * Symbol.toPrimitive]. You can add other properties to specify that they should be run locally
   * on the javascript host object proxy.
   */
  forceLocalProperties: string[];

  /**
   * By default, an exception is thrown when attempting to get the value of a proxy property
   * that doesn't exist on the corresponding native class. Setting this property to true changes
   * the behavior to match Chakra WinRT projection (and general JavaScript) behavior of returning
   * undefined with no error.
   */
  ignoreMemberNotFoundError: boolean;

  /**
   * This is a callback delegate that will be called with debug information if non-null.
   * 
   * For example, you can set this to console.log.bind(console) to have it print debug information
   * to the console, to help when troubleshooting host object usage.
   * 
   * By default this is null.
   * @param data 
   */
  log: (...data: any[]) => void;

  /**
   * By default this is false, and javascript Date objects will be sent to host objects as a
   * string using JSON.stringify.
   * 
   * You can set this property to true to have Date objects properly serialize as a VT_DATE
   * when sending to the native host object, and have VT_DATE properties and return values
   * create a JavaScript Date object.
   */
  shouldSerializeDates: boolean;
}

/**
 * Contains synchronous proxies for all host objects added via CoreWebView2.AddHostObjectToScript.
 * 
 * If you call coreWebView2.AddHostObjectToScript("myObject", object); in your native code, a
 * synchronous proxy for object is available to your web-side code, by using
 * chrome.webview.hostObjects.sync.myObject.
 */
export interface HostObjectsSyncRoot {
  [key: string]: HostObjectSyncProxy;
}

/**
 * A synchronous host object proxy. Host objects added via CoreWebView2.AddHostObjectToScript
 * are exposed as host object proxies using window.chrome.webview.hostObjects.{name}. A host
 * object proxy represent a host object.
 * 
 * Host object proxies are JavaScript Proxy objects that intercept all property get, property
 * set, and method invocations. Properties or methods that are a part of the Function or Object
 * prototype are run locally. Additionally any property or method in the
 * chrome.webview.hostObjects.options.forceLocalProperties array is also run locally. This
 * defaults to including optional methods that have meaning in JavaScript like toJSON and
 * Symbol.toPrimitive. Add more to the array as required.
 */
export interface HostObjectSyncProxy extends CallableFunction {
  [key: string]: any;

  /**
   * Perform a method invocation on the host object that corresponds to this proxy.
   * All parameters are converted to call the host object method.
   * @param argArray An array of arguments to pass to the host object method invocation.
   * @returns The converted value of the return value of the host object method invocation.
   */
  applyHostFunction(argArray?: any): any;

  /**
   * A method which blocks and returns an asynchronous host object proxy for the same host
   * object. For example, chrome.webview.hostObjects.sync.sample.methodCall() returns a
   * synchronous host object proxy. Running the async method on this blocks and then returns
   * an asynchronous host object proxy for the same host object:
   * const asyncProxy = chrome.webview.hostObjects.sync.sample.methodCall().async().
   * @returns An asynchronous host object proxy for the same host object.
   */
  async(): HostObjectAsyncProxy;

  /**
   * Perform a property get on the host object. Use this method to explicitly force a property
   * get to occur remotely if a conflicting local method or property exists. For instance,
   * proxy.toString() runs the local toString method on the proxy object. But
   * proxy.applyHostFunction('toString') runs toString on the host proxied object instead.
   * @param propertyName String name of the property of which to get the value.
   * @returns The converted value of the property of the host object's property.
   */
  getHostProperty(propertyName: string): any;

  /**
   * Perform a property get locally on the proxy object. Use the methods to force getting a
   * property on the host object proxy rather than on the host object it represents. For
   * instance, proxy.unknownProperty gets the property named unknownProperty from the host
   * proxied object. But proxy.getLocalProperty('unknownProperty') gets the value of the
   * property unknownProperty on the proxy object.
   * @param propertyName Name of the property to get the value of.
   * @returns The value of the property.
   */
  getLocalProperty(propertyName: string): any;

  /**
   * Perform a property set on the host object. Use this method to explicitly force a property
   * set to occur remotely if a conflicting local method or property exists.
   * @param propertyName Name of the property of which to set the value.
   * @param propertyValue Value to set the property.
   * @returns The converted value of the property of the host object's property.
   */
  setHostProperty(propertyName: string, propertyValue: any): any;

  /**
   * Perform a property set locally on the proxy object. Use the methods to force setting a
   * property on the host object proxy rather than on the host object it represents. For
   * instance, proxy.unknownProperty = 2 sets the property named unknownProperty on the host
   * proxied object. But proxy.setLocalProperty('unknownProperty', 2) sets the value of the
   * property unknownProperty on the proxy object.
   * @param propertyName Name of the property to get the value of.
   * @param propertyValue Value to set the property to.
   * @returns The value of the property after it is set.
   */
  setLocalProperty(propertyName: string, propertyValue: any): any;
}

/**
 * Event object for the chrome.webview.sharedbufferreceived event. This event is dispatched
 * when CoreWebView2.PostSharedBufferToScript is successfully called.
 */
export interface SharedBufferReceivedEvent extends Event {
  /**
   * An object that is the result of parsing the additionalDataAsJson parameter to
   * CoreWebView2.PostSharedBufferToScript as a JSON string. This property will be undefined
   * if additionalDataAsJson is nullptr or the empty string.
   */
  additionalData: any;

  /**
   * The source of the event is the chrome.webview object.
   */
  source: WebView;

  /**
   * Returns an ArrayBuffer object with the backing content from the shared buffer passed to
   * CoreWebView2.PostSharedBufferToScript. If CoreWebView2.PostSharedBufferToScript was
   * called with the buffer set to ReadOnly, then only read access is allowed to the buffer.
   * If you try to modify the content in a read-only buffer, it will cause an access violation
   * in the WebView renderer process and crash the renderer process. 
   * @returns An ArrayBuffer over the shared buffer passed to
   *          CoreWebView2.PostSharedBufferToScript
   */
  getBuffer(): ArrayBuffer;
}

interface WebViewEventListenerObject {
  handleEvent(object: Event & { data?: any }): void;
}

interface WebViewEventListener {
  (evt: Event & { data?: any }): void;
}

/**
 * WebView2 augments the standard DOM event listener event with an additional data field. This field
 * contains any JSON encoded passed from the host via PostWebMessageAsJson.
 * See https://learn.microsoft.com/en-us/microsoft-edge/webview2/reference/win32/icorewebview2?view=webview2-1.0.1722.45#postwebmessageasjson
 */
type WebViewEventListenerOrEventListenerObject = WebViewEventListener | WebViewEventListenerObject;

/**
 * window.chrome.webview is the class to access the WebView2-specific APIs that are available
 * to the script running within WebView2 Runtime.
 */
export interface WebView extends EventTarget {
  /**
   * Contains asynchronous proxies for all host objects added via CoreWebView2.AddHostObjectToScript
   * as well as options to configure those proxies, and the container for synchronous proxies.
   * 
   * If you call coreWebView2.AddHostObjectToScript("myObject", object); in your native code,
   * an asynchronous proxy for object is available to your web-side code, by using
   * chrome.webview.hostObjects.myObject.
   */
  hostObjects: HostObjectsAsyncRoot;

  /**
   * The standard EventTarget.addEventListener method. Use it to subscribe to the message event
   * or sharedbufferreceived event. The message event receives messages posted from the WebView2
   * host via CoreWebView2.PostWebMessageAsJson or CoreWebView2.PostWebMessageAsString. The
   * sharedbufferreceived event receives shared buffers posted from the WebView2 host via
   * CoreWebView2.PostSharedBufferToScript.
   * See CoreWebView2.PostWebMessageAsJson( Win32/C++, .NET, WinRT).
   * @param type The name of the event to subscribe to. Valid values are message, and sharedbufferreceived.
   * @param listener The callback to invoke when the event is raised.
   * @param options Options to control how the event is handled.
   */
  addEventListener(
    type: string,
    listener: WebViewEventListenerOrEventListenerObject,
    options?: boolean | AddEventListenerOptions): void;

  /**
   * When the page calls postMessage, the message parameter is converted to JSON and is posted
   * asynchronously to the WebView2 host process. This will result in either the
   * CoreWebView2.WebMessageReceived event or the CoreWebView2Frame.WebMessageReceived event being
   * raised, depending on if postMessage is called from the top-level document in the WebView2
   * or from a child frame. See CoreWebView2.WebMessageReceived( Win32/C++, .NET, WinRT).
   * See CoreWebView2Frame.WebMessageReceived( Win32/C++, .NET, WinRT).
   * @param message The message to send to the WebView2 host. This can be any object that can be
   *                serialized to JSON.
   */
  postMessage(message: any): void;

  /**
   * Call with the ArrayBuffer from the chrome.webview.sharedbufferreceived event to release the
   * underlying shared memory resource.
   * @param buffer An ArrayBuffer from the chrome.webview.sharedbufferreceived event.
   */
  releaseBuffer(buffer: ArrayBuffer): void;

  /**
   * The standard EventTarget.removeEventListener method. Use it to unsubscribe to the message
   * or sharedbufferreceived event.
   * @param type The name of the event to unsubscribe from. Valid values are message and sharedbufferreceived.
   * @param listener The callback to remove from the event.
   * @param options Options to control how the event is handled.
   */
  removeEventListener(
    type: string,
    listener: WebViewEventListenerOrEventListenerObject,
    options?: boolean | EventListenerOptions): void;
}

// Global object
declare global {
  interface Window {
    chrome: {
      webview: WebView;
    };
  }
}