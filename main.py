import fnmatch
import os
import uuid
from datetime import date as dt_date
from datetime import datetime, timedelta
from typing import Generator, List, Optional

from fastapi import Depends, FastAPI, HTTPException, Query, status
from fastapi.security import OAuth2PasswordBearer, OAuth2PasswordRequestForm
from jose import JWTError, jwt
from passlib.context import CryptContext
from pydantic import BaseModel, Field
from sqlalchemy import Date, Float, ForeignKey, Integer, String, create_engine
from sqlalchemy.exc import IntegrityError
from sqlalchemy.orm import DeclarativeBase, Mapped, Session, mapped_column, relationship, sessionmaker

SECRET_KEY = "fitness-tracker-secret-key-2024"
ALGORITHM = "HS256"
ACCESS_TOKEN_EXPIRE_MINUTES = 60
DATABASE_URL = os.getenv(
    "DATABASE_URL",
    "postgresql+psycopg://fitness_user:fitness_password@localhost:5432/fitness_tracker",
)

app = FastAPI(
    title="Fitness Tracker API",
    description="REST API для фитнес-трекера с хранением данных в PostgreSQL",
    version="2.0.0",
)

pwd_context = CryptContext(schemes=["pbkdf2_sha256"], deprecated="auto")
oauth2_scheme = OAuth2PasswordBearer(tokenUrl="token")


class Base(DeclarativeBase):
    pass


def get_engine():
    return create_engine(DATABASE_URL, future=True)


engine = get_engine()
SessionLocal = sessionmaker(bind=engine, autoflush=False, autocommit=False, future=True)


class User(Base):
    __tablename__ = "users"

    id: Mapped[str] = mapped_column(String(36), primary_key=True, default=lambda: str(uuid.uuid4()))
    username: Mapped[str] = mapped_column(String(50), unique=True, nullable=False)
    first_name: Mapped[str] = mapped_column(String(100), nullable=False)
    last_name: Mapped[str] = mapped_column(String(100), nullable=False)
    email: Mapped[str] = mapped_column(String(255), unique=True, nullable=False)
    hashed_password: Mapped[str] = mapped_column(String(255), nullable=False)

    workouts: Mapped[List["Workout"]] = relationship(back_populates="user", cascade="all, delete-orphan")


class Exercise(Base):
    __tablename__ = "exercises"

    id: Mapped[str] = mapped_column(String(36), primary_key=True, default=lambda: str(uuid.uuid4()))
    name: Mapped[str] = mapped_column(String(200), nullable=False)
    description: Mapped[str] = mapped_column(String(1000), default="", nullable=False)
    muscle_group: Mapped[str] = mapped_column(String(100), nullable=False)

    workout_links: Mapped[List["WorkoutExercise"]] = relationship(back_populates="exercise")


class Workout(Base):
    __tablename__ = "workouts"

    id: Mapped[str] = mapped_column(String(36), primary_key=True, default=lambda: str(uuid.uuid4()))
    user_id: Mapped[str] = mapped_column(String(36), ForeignKey("users.id"), nullable=False)
    name: Mapped[str] = mapped_column(String(200), nullable=False)
    date: Mapped[dt_date] = mapped_column("workout_date", Date, nullable=False, default=dt_date.today)

    user: Mapped[User] = relationship(back_populates="workouts")
    exercises: Mapped[List["WorkoutExercise"]] = relationship(
        back_populates="workout",
        cascade="all, delete-orphan",
    )


class WorkoutExercise(Base):
    __tablename__ = "workout_exercises"

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    workout_id: Mapped[str] = mapped_column(String(36), ForeignKey("workouts.id"), nullable=False)
    exercise_id: Mapped[str] = mapped_column(String(36), ForeignKey("exercises.id"), nullable=False)
    sets: Mapped[int] = mapped_column(Integer, nullable=False)
    reps: Mapped[int] = mapped_column(Integer, nullable=False)
    weight: Mapped[float] = mapped_column(Float, nullable=False, default=0)

    workout: Mapped[Workout] = relationship(back_populates="exercises")
    exercise: Mapped[Exercise] = relationship(back_populates="workout_links")


def init_db() -> None:
    Base.metadata.create_all(bind=engine)


@app.on_event("startup")
def on_startup() -> None:
    init_db()


def get_db() -> Generator[Session, None, None]:
    db = SessionLocal()
    try:
        yield db
    finally:
        db.close()


class UserCreate(BaseModel):
    username: str = Field(..., min_length=3, max_length=50)
    first_name: str = Field(..., min_length=1, max_length=100)
    last_name: str = Field(..., min_length=1, max_length=100)
    email: str = Field(..., pattern=r"^[\w\.-]+@[\w\.-]+\.\w+$")
    password: str = Field(..., min_length=6)


class UserResponse(BaseModel):
    id: str
    username: str
    first_name: str
    last_name: str
    email: str


class ExerciseCreate(BaseModel):
    name: str = Field(..., min_length=1, max_length=200)
    description: str = Field(default="", max_length=1000)
    muscle_group: str = Field(..., min_length=1, max_length=100)


class ExerciseResponse(BaseModel):
    id: str
    name: str
    description: str
    muscle_group: str


class WorkoutExerciseAdd(BaseModel):
    exercise_id: str
    sets: int = Field(..., ge=1, le=100)
    reps: int = Field(..., ge=1, le=1000)
    weight: float = Field(default=0, ge=0)


class WorkoutExerciseResponse(BaseModel):
    exercise_id: str
    exercise_name: str
    sets: int
    reps: int
    weight: float


class WorkoutCreate(BaseModel):
    name: str = Field(..., min_length=1, max_length=200)
    date: Optional[str] = None


class WorkoutResponse(BaseModel):
    id: str
    user_id: str
    name: str
    date: str
    exercises: List[WorkoutExerciseResponse]


class WorkoutStats(BaseModel):
    user_id: str
    start_date: str
    end_date: str
    total_workouts: int
    total_exercises: int
    total_sets: int
    total_reps: int
    workouts: List[WorkoutResponse]


class Token(BaseModel):
    access_token: str
    token_type: str


def create_access_token(data: dict, expires_delta: Optional[timedelta] = None) -> str:
    to_encode = data.copy()
    expire = datetime.utcnow() + (expires_delta or timedelta(minutes=15))
    to_encode.update({"exp": expire})
    return jwt.encode(to_encode, SECRET_KEY, algorithm=ALGORITHM)


def to_user_response(user: User) -> UserResponse:
    return UserResponse(
        id=user.id,
        username=user.username,
        first_name=user.first_name,
        last_name=user.last_name,
        email=user.email,
    )


def to_exercise_response(exercise: Exercise) -> ExerciseResponse:
    return ExerciseResponse(
        id=exercise.id,
        name=exercise.name,
        description=exercise.description,
        muscle_group=exercise.muscle_group,
    )


def to_workout_response(workout: Workout) -> WorkoutResponse:
    return WorkoutResponse(
        id=workout.id,
        user_id=workout.user_id,
        name=workout.name,
        date=workout.date.isoformat(),
        exercises=[
            WorkoutExerciseResponse(
                exercise_id=item.exercise_id,
                exercise_name=item.exercise.name,
                sets=item.sets,
                reps=item.reps,
                weight=item.weight,
            )
            for item in workout.exercises
        ],
    )


def parse_iso_date(raw_date: Optional[str]) -> dt_date:
    if raw_date is None:
        return dt_date.today()
    try:
        return datetime.strptime(raw_date, "%Y-%m-%d").date()
    except ValueError as exc:
        raise HTTPException(status_code=400, detail="Date must be in YYYY-MM-DD format") from exc


async def get_current_user(
    token: str = Depends(oauth2_scheme),
    db: Session = Depends(get_db),
) -> User:
    credentials_exception = HTTPException(
        status_code=status.HTTP_401_UNAUTHORIZED,
        detail="Could not validate credentials",
        headers={"WWW-Authenticate": "Bearer"},
    )
    try:
        payload = jwt.decode(token, SECRET_KEY, algorithms=[ALGORITHM])
        username = payload.get("sub")
        if username is None:
            raise credentials_exception
    except JWTError as exc:
        raise credentials_exception from exc

    user = db.query(User).filter(User.username == username).first()
    if user is None:
        raise credentials_exception
    return user


def create_user_record(user_data: UserCreate, db: Session) -> User:
    user = User(
        username=user_data.username,
        first_name=user_data.first_name,
        last_name=user_data.last_name,
        email=user_data.email,
        hashed_password=pwd_context.hash(user_data.password),
    )
    db.add(user)
    try:
        db.commit()
    except IntegrityError as exc:
        db.rollback()
        error_message = str(exc.orig).lower()
        detail = "Username or email already exists"
        if "username" in error_message:
            detail = "Username already exists"
        elif "email" in error_message:
            detail = "Email already exists"
        raise HTTPException(status_code=409, detail=detail) from exc
    db.refresh(user)
    return user


@app.post(
    "/register",
    response_model=UserResponse,
    status_code=status.HTTP_201_CREATED,
    tags=["Auth"],
    summary="Регистрация нового пользователя",
)
def register(user_data: UserCreate, db: Session = Depends(get_db)):
    return to_user_response(create_user_record(user_data, db))


@app.post("/token", response_model=Token, tags=["Auth"], summary="Получение JWT токена (логин)")
def login(form_data: OAuth2PasswordRequestForm = Depends(), db: Session = Depends(get_db)):
    user = db.query(User).filter(User.username == form_data.username).first()
    if user is None or not pwd_context.verify(form_data.password, user.hashed_password):
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Incorrect username or password",
            headers={"WWW-Authenticate": "Bearer"},
        )

    access_token = create_access_token(
        data={"sub": user.username},
        expires_delta=timedelta(minutes=ACCESS_TOKEN_EXPIRE_MINUTES),
    )
    return {"access_token": access_token, "token_type": "bearer"}


@app.post(
    "/users",
    response_model=UserResponse,
    status_code=status.HTTP_201_CREATED,
    tags=["Users"],
    summary="Создание нового пользователя",
)
def create_user(user_data: UserCreate, db: Session = Depends(get_db)):
    return to_user_response(create_user_record(user_data, db))


@app.get(
    "/users/search",
    response_model=List[UserResponse],
    tags=["Users"],
    summary="Поиск пользователей по логину или маске имени/фамилии",
)
def search_users(
    username: Optional[str] = Query(None, description="Поиск по логину (exact)"),
    first_name: Optional[str] = Query(None, description="Маска имени (* для wildcard)"),
    last_name: Optional[str] = Query(None, description="Маска фамилии (* для wildcard)"),
    db: Session = Depends(get_db),
):
    if not username and not first_name and not last_name:
        raise HTTPException(status_code=400, detail="Provide at least one search parameter")

    if username:
        users = db.query(User).filter(User.username == username).all()
    else:
        users = db.query(User).all()
        users = [
            user
            for user in users
            if (
                (first_name is None or fnmatch.fnmatch(user.first_name.lower(), first_name.lower()))
                and (last_name is None or fnmatch.fnmatch(user.last_name.lower(), last_name.lower()))
            )
        ]

    return [to_user_response(user) for user in users]


@app.post(
    "/exercises",
    response_model=ExerciseResponse,
    status_code=status.HTTP_201_CREATED,
    tags=["Exercises"],
    summary="Создание упражнения",
)
def create_exercise(
    exercise_data: ExerciseCreate,
    current_user: User = Depends(get_current_user),
    db: Session = Depends(get_db),
):
    del current_user
    exercise = Exercise(
        name=exercise_data.name,
        description=exercise_data.description,
        muscle_group=exercise_data.muscle_group,
    )
    db.add(exercise)
    db.commit()
    db.refresh(exercise)
    return to_exercise_response(exercise)


@app.get(
    "/exercises",
    response_model=List[ExerciseResponse],
    tags=["Exercises"],
    summary="Получение списка упражнений",
)
def get_exercises(db: Session = Depends(get_db)):
    exercises = db.query(Exercise).order_by(Exercise.name.asc()).all()
    return [to_exercise_response(exercise) for exercise in exercises]


@app.post(
    "/users/{user_id}/workouts",
    response_model=WorkoutResponse,
    status_code=status.HTTP_201_CREATED,
    tags=["Workouts"],
    summary="Создание тренировки",
)
def create_workout(
    user_id: str,
    workout_data: WorkoutCreate,
    current_user: User = Depends(get_current_user),
    db: Session = Depends(get_db),
):
    del current_user
    user = db.query(User).filter(User.id == user_id).first()
    if user is None:
        raise HTTPException(status_code=404, detail="User not found")

    workout = Workout(user_id=user_id, name=workout_data.name, date=parse_iso_date(workout_data.date))
    db.add(workout)
    db.commit()
    db.refresh(workout)
    return to_workout_response(workout)


@app.post(
    "/users/{user_id}/workouts/{workout_id}/exercises",
    response_model=WorkoutResponse,
    tags=["Workouts"],
    summary="Добавление упражнения в тренировку",
)
def add_exercise_to_workout(
    user_id: str,
    workout_id: str,
    exercise_data: WorkoutExerciseAdd,
    current_user: User = Depends(get_current_user),
    db: Session = Depends(get_db),
):
    del current_user
    user = db.query(User).filter(User.id == user_id).first()
    if user is None:
        raise HTTPException(status_code=404, detail="User not found")

    workout = db.query(Workout).filter(Workout.id == workout_id).first()
    if workout is None:
        raise HTTPException(status_code=404, detail="Workout not found")
    if workout.user_id != user_id:
        raise HTTPException(status_code=403, detail="Workout does not belong to this user")

    exercise = db.query(Exercise).filter(Exercise.id == exercise_data.exercise_id).first()
    if exercise is None:
        raise HTTPException(status_code=404, detail="Exercise not found")

    workout_exercise = WorkoutExercise(
        workout_id=workout.id,
        exercise_id=exercise.id,
        sets=exercise_data.sets,
        reps=exercise_data.reps,
        weight=exercise_data.weight,
    )
    db.add(workout_exercise)
    db.commit()
    db.refresh(workout)
    return to_workout_response(workout)


@app.get(
    "/users/{user_id}/workouts",
    response_model=List[WorkoutResponse],
    tags=["Workouts"],
    summary="Получение истории тренировок пользователя",
)
def get_user_workouts(
    user_id: str,
    current_user: User = Depends(get_current_user),
    db: Session = Depends(get_db),
):
    del current_user
    user = db.query(User).filter(User.id == user_id).first()
    if user is None:
        raise HTTPException(status_code=404, detail="User not found")

    workouts = (
        db.query(Workout)
        .filter(Workout.user_id == user_id)
        .order_by(Workout.date.desc(), Workout.name.asc())
        .all()
    )
    return [to_workout_response(workout) for workout in workouts]


@app.get(
    "/users/{user_id}/workouts/stats",
    response_model=WorkoutStats,
    tags=["Workouts"],
    summary="Получение статистики тренировок за период",
)
def get_workout_stats(
    user_id: str,
    start_date: str = Query(..., description="Начальная дата (YYYY-MM-DD)"),
    end_date: str = Query(..., description="Конечная дата (YYYY-MM-DD)"),
    current_user: User = Depends(get_current_user),
    db: Session = Depends(get_db),
):
    del current_user
    user = db.query(User).filter(User.id == user_id).first()
    if user is None:
        raise HTTPException(status_code=404, detail="User not found")

    start = parse_iso_date(start_date)
    end = parse_iso_date(end_date)

    workouts = (
        db.query(Workout)
        .filter(Workout.user_id == user_id, Workout.date >= start, Workout.date <= end)
        .order_by(Workout.date.asc(), Workout.name.asc())
        .all()
    )

    total_exercises = sum(len(workout.exercises) for workout in workouts)
    total_sets = sum(item.sets for workout in workouts for item in workout.exercises)
    total_reps = sum(item.reps * item.sets for workout in workouts for item in workout.exercises)

    return WorkoutStats(
        user_id=user_id,
        start_date=start.isoformat(),
        end_date=end.isoformat(),
        total_workouts=len(workouts),
        total_exercises=total_exercises,
        total_sets=total_sets,
        total_reps=total_reps,
        workouts=[to_workout_response(workout) for workout in workouts],
    )


if __name__ == "__main__":
    import uvicorn

    init_db()
    uvicorn.run(app, host="0.0.0.0", port=8000)
