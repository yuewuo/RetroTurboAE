import numpy as np
import cv2 as cv

chessboard = (6,9)

# termination criteria
criteria = (cv.TERM_CRITERIA_EPS + cv.TERM_CRITERIA_MAX_ITER, 30, 0.001)
# prepare object points, like (0,0,0), (1,0,0), (2,0,0) ....,(6,5,0)
objp = np.zeros((chessboard[0]*chessboard[1],3), np.float32)
objp[:,:2] = np.mgrid[0:chessboard[0],0:chessboard[1]].T.reshape(-1,2)
# Arrays to store object points and image points from all the images.
objpoints = [] # 3d point in real world space
imgpoints = [] # 2d points in image plane.
fname = "chessboard_1920x1080.jpg"
img = cv.imread(fname)
gray = cv.cvtColor(img, cv.COLOR_BGR2GRAY)
# Find the chess board corners
ret, corners = cv.findChessboardCorners(gray, chessboard, None)
# If found, add object points, image points (after refining them)
assert(ret == True)  # chass board not found, please adjust "chessboard" parameter
objpoints.append(objp)
corners2 = cv.cornerSubPix(gray,corners, (11,11), (-1,-1), criteria)
imgpoints.append(corners)
# Draw and display the corners
cv.drawChessboardCorners(img, chessboard, corners2, ret)
cv.imshow('img', img)
# cv.waitKey()

print(gray.shape[::-1])
ret, mtx, dist, rvecs, tvecs = cv.calibrateCamera(objpoints, imgpoints, gray.shape[::-1], None, None)

img2 = cv.imread(fname)
h, w = img2.shape[:2]
newcameramtx, roi = cv.getOptimalNewCameraMatrix(mtx, dist, (w,h), 1, (w,h))

print(mtx)
print(dist)
# dst = cv.undistort(img2, mtx, dist, None, newcameramtx)
dst = cv.undistort(img2, mtx, dist)
# x, y, w, h = roi  # failed, roi is all 0
# dst = dst[y:y+h, x:x+w]
cv.imshow('dst', dst)
cv.waitKey()

# cv.destroyAllWindows()
