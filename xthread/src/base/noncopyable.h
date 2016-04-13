/*
 * Noncopyable.h
 *
 *  Created on: 2016Äê3ÔÂ23ÈÕ
 *      Author: Administrator
 */

#ifndef BASE_NONCOPYABLE_H_
#define BASE_NONCOPYABLE_H_

namespace xthread
{
namespace base
{

 class NonCopyable
 {
     public:
         NonCopyable(){}
         ~NonCopyable(){}
     private:
         NonCopyable(const NonCopyable &);
         const NonCopyable& operator=(const NonCopyable &);
};

}
}



#endif /* BASE_NONCOPYABLE_H_ */
