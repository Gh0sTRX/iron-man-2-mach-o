# Original Iron Man 2 ObjC classref binding inventory

Derived from owner-provided original decrypted 32-bit ARM Mach-O, without uploading executable or assets.

**Verified**: 243 `__objc_classrefs` slots = 25 initialized local slots + 218 zero slots. Exactly 218 external relocation records target those zero slots, naming **64 unique external classes**. The full external relocation table also contains other records outside this section; do not equate all class-symbol relocations with classrefs.

| Class name | External classref slots |
|---|---:|
| NSString | 22 |
| NSAutoreleasePool | 14 |
| UIDevice | 13 |
| UIColor | 11 |
| NSURL | 10 |
| UIAlertView | 8 |
| UIApplication | 8 |
| UIActivityIndicatorView | 7 |
| UIFont | 7 |
| UIView | 7 |
| NSData | 6 |
| UILabel | 6 |
| NSLocale | 5 |
| NSURLConnection | 5 |
| UIButton | 5 |
| UIImageView | 5 |
| NSBundle | 4 |
| NSMutableData | 4 |
| NSTimer | 4 |
| NSURLRequest | 4 |
| UIImage | 4 |
| NSNotificationCenter | 3 |
| NSThread | 3 |
| SKPaymentQueue | 3 |
| EAGLContext | 2 |
| MPMusicPlayerController | 2 |
| NSFileManager | 2 |
| NSMutableString | 2 |
| NSMutableURLRequest | 2 |
| NSSet | 2 |
| UITableView | 2 |
| UITableViewCell | 2 |
| UITextView | 2 |
| UIViewController | 2 |
| CAEAGLLayer | 1 |
| MPMediaItemCollection | 1 |
| MPMediaLibrary | 1 |
| MPMediaQuery | 1 |
| MPMoviePlayerController | 1 |
| NSArray | 1 |
| NSAssertionHandler | 1 |
| NSCharacterSet | 1 |
| NSDate | 1 |
| NSDateFormatter | 1 |
| NSDictionary | 1 |
| NSIndexPath | 1 |
| NSMutableArray | 1 |
| NSMutableDictionary | 1 |
| NSNumber | 1 |
| NSNumberFormatter | 1 |
| NSPredicate | 1 |
| NSRunLoop | 1 |
| NSScanner | 1 |
| NSUserDefaults | 1 |
| SKPayment | 1 |
| SKProductsRequest | 1 |
| UIAccelerometer | 1 |
| UIImagePickerController | 1 |
| UIScreen | 1 |
| UIScrollView | 1 |
| UITabBarController | 1 |
| UITextField | 1 |
| UIWebView | 1 |
| UIWindow | 1 |

## Implementation order
1. Objective-C runtime data representation: class/metaclass identity, selector interning, isa/superclass dispatch, retain/release/autorelease pools.
2. Foundation essentials: NSString, NSData, collections, NSBundle, NSURL, NSFileManager, NSTimer.
3. UIKit lifecycle and drawing: UIApplication, UIView, UIWindow, controls, text, screen, orientation, input.
4. Graphics: EAGLContext/CAEAGLLayer with VitaGL.
5. Optional/online legacy services: StoreKit, media player/library, web view, accelerometer; retain explicit failure/unsupported semantics, not fake success.

A classref binding is **not** implemented merely because a symbol name resolves. Each binding needs a valid runtime object and ABI-compatible methods.
